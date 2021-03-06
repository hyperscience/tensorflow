/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/compiler/mlir/lite/python/graphdef_to_tfl_flatbuffer.h"

#include <ostream>

#include "llvm/Support/ToolOutputFile.h"
#include "mlir/IR/MLIRContext.h"  // TF:local_config_mlir
#include "mlir/IR/Module.h"  // TF:local_config_mlir
#include "mlir/Pass/Pass.h"  // TF:local_config_mlir
#include "mlir/Support/FileUtilities.h"  // TF:local_config_mlir
#include "mlir/Transforms/ViewOpGraph.h"  // TF:local_config_mlir
#include "tensorflow/compiler/mlir/lite/common/tfl_pass_config.h"
#include "tensorflow/compiler/mlir/lite/tf_tfl_passes.h"
#include "tensorflow/compiler/mlir/lite/tf_to_tfl_flatbuffer.h"
#include "tensorflow/compiler/mlir/tensorflow/translate/import_model.h"
#include "tensorflow/compiler/mlir/tensorflow/translate/mlir_roundtrip_flags.h"
#include "tensorflow/core/framework/graph.pb.h"
#include "tensorflow/core/framework/types.pb.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/protobuf/graph_debug_info.pb.h"
#include "tensorflow/lite/toco/model_flags.pb.h"
#include "tensorflow/lite/toco/toco_flags.pb.h"
#include "tensorflow/lite/toco/types.pb.h"

namespace tensorflow {

namespace {
// Converts the toco::IODataType to tensorflow::DataType. Only contains the
// conversion mapping for constants defined in TFLite Python API.
DataType ConvertIODataTypeToDataType(toco::IODataType dtype) {
  switch (dtype) {
    case toco::IODataType::FLOAT:
      return DT_FLOAT;
    case toco::IODataType::QUANTIZED_UINT8:
      return DT_QUINT8;
    case toco::IODataType::INT8:
      return DT_QINT8;
    case toco::IODataType::INT32:
      return DT_INT32;
    case toco::IODataType::INT64:
      return DT_INT64;
    case toco::IODataType::STRING:
      return DT_STRING;
    case toco::IODataType::BOOL:
      return DT_BOOL;
    default:
      return DT_INVALID;
  }
}

// Give a warning for any unused flags that have been specified.
void WarningUnusedFlags(const toco::ModelFlags& model_flags,
                        const toco::TocoFlags& toco_flags) {
  if (toco_flags.output_format()) {
    LOG(WARNING) << "Ignored output_format.";
  }
  if (toco_flags.default_ranges_min() || toco_flags.default_ranges_max()) {
    LOG(WARNING) << "Ignored default_ranges_stats.";
  }
  if (toco_flags.drop_control_dependency()) {
    LOG(WARNING) << "Ignored drop_control_dependency.";
  }
  if (toco_flags.reorder_across_fake_quant()) {
    LOG(WARNING) << "Ignored reorder_across_fake_quant.";
  }
  if (model_flags.change_concat_input_ranges()) {
    LOG(WARNING) << "Ignored change_concat_input_ranges.";
  }
  if (toco_flags.dump_graphviz_include_video()) {
    LOG(WARNING) << "Ignored dump_graphviz_video.";
  }
  if (model_flags.allow_nonexistent_arrays()) {
    LOG(WARNING) << "Allow allow_nonexistent_arrays.";
  }
}

// Dumps the op graph of the `module` to `filename` in DOT format.
Status DumpOpGraphToFile(mlir::ModuleOp module, const std::string& filename) {
  std::string error_message;
  auto output = mlir::openOutputFile(filename, &error_message);
  if (!error_message.empty()) {
    return errors::InvalidArgument("Failed to open file in %s.", filename);
  }
  mlir::PassManager pm(module.getContext());
  pm.addPass(mlir::createPrintOpGraphPass(output->os()));
  if (failed(pm.run(module))) {
    return errors::Unknown("Failed to dump Op Graph from MLIR module.");
  }
  output->keep();
  return Status::OK();
}

}  // namespace

Status ConvertGraphDefToTFLiteFlatBuffer(const toco::ModelFlags& model_flags,
                                         const toco::TocoFlags& toco_flags,
                                         const GraphDebugInfo& debug_info,
                                         const GraphDef& input,
                                         string* result) {
  mlir::MLIRContext context;
  GraphImportConfig specs;
  mlir::TFL::QuantizationSpecs quant_specs;

  // Parse input arrays.
  std::vector<string> node_names;
  std::vector<string> node_dtypes;
  std::vector<std::vector<int>> node_shapes;
  std::vector<double> node_mins;
  std::vector<double> node_maxs;
  quant_specs.inference_input_type =
      ConvertIODataTypeToDataType(toco_flags.inference_input_type());
  tensorflow::DataType inference_type =
      ConvertIODataTypeToDataType(toco_flags.inference_type());
  // Use non-float flag `inference_input_type` to override the `inference_type`
  // because we have to apply quantization to satisfy that.
  if (quant_specs.inference_input_type != tensorflow::DT_FLOAT) {
    inference_type = quant_specs.inference_input_type;
  }

  // Build a map from placeholder to data types.
  llvm::StringMap<DataType> placeholder_data_type_map;
  for (const NodeDef& node_def : input.node()) {
    if (node_def.op() == "Placeholder" && node_def.attr().count("dtype") > 0) {
      placeholder_data_type_map[node_def.name()] =
          node_def.attr().at("dtype").type();
    }
  }

  for (auto& flag : model_flags.input_arrays()) {
    // TOCO doesn't required `data_type` to be filled for every input.
    // If it's not filled, try to get the data type from the placeholder.
    auto toco_data_type = flag.data_type();
    DataType data_type;
    if (toco_data_type == ::toco::IODataType::IO_DATA_TYPE_UNKNOWN &&
        placeholder_data_type_map.find(flag.name()) !=
            placeholder_data_type_map.end()) {
      data_type = placeholder_data_type_map[flag.name()];
    } else {
      data_type = ConvertIODataTypeToDataType(toco_data_type);
    }
    node_names.push_back(flag.name());
    node_dtypes.push_back(DataType_Name(data_type));
    node_shapes.push_back(std::vector<int>(flag.shape().dims().begin(),
                                           flag.shape().dims().end()));

    const double mean_value = flag.mean_value();
    const double std_value = flag.std_value();
    const double qmin = 0.0, qmax = 255.0;
    node_mins.push_back((qmin - mean_value) / std_value);
    node_maxs.push_back((qmax - mean_value) / std_value);
  }
  TF_RETURN_IF_ERROR(tensorflow::ParseInputArrayInfo(
      node_names, node_dtypes, node_shapes, &specs.inputs));
  if (mlir::TFL::GetInputNodeQuantSpecs(node_names, node_mins, node_maxs,
                                        inference_type, &quant_specs)) {
    return errors::InvalidArgument("Failed to get input quant spec.");
  }

  // Some extra flag related to post training quantization. If post-training
  // quantization is enabled, `inference_type` and `inference_input_type` are
  // not used by MLIR passes.
  if (toco_flags.post_training_quantize()) {
    quant_specs.weight_quantization = true;
    if (toco_flags.quantize_to_float16()) {
      quant_specs.inference_type = tensorflow::DT_HALF;
      quant_specs.inference_input_type = tensorflow::DT_HALF;
    } else {
      quant_specs.inference_type = tensorflow::DT_QINT8;
      quant_specs.inference_input_type = tensorflow::DT_QINT8;
    }
  }

  // Parse output arrays.
  std::vector<string> output_arrays(model_flags.output_arrays().begin(),
                                    model_flags.output_arrays().end());
  TF_RETURN_IF_ERROR(tensorflow::ParseOutputArrayInfo(
      output_arrays, &specs.output_arrays, &specs.output_arrays_order));

  // Other flags.
  bool emit_builtin_tflite_ops = !toco_flags.force_select_tf_ops();
  bool emit_select_tf_ops = toco_flags.enable_select_tf_ops();
  bool emit_custom_ops = toco_flags.allow_custom_ops();
  specs.prune_unused_nodes = true;
  specs.convert_legacy_fed_inputs = true;
  specs.graph_as_function = false;
  specs.upgrade_legacy = true;
  WarningUnusedFlags(model_flags, toco_flags);

  TF_ASSIGN_OR_RETURN(
      auto module, ConvertGraphdefToMlir(input, debug_info, specs, &context));

  if (toco_flags.has_dump_graphviz_dir()) {
    TF_RETURN_IF_ERROR(DumpOpGraphToFile(
        module.get(),
        // rename once we enable the new converter feature flag.
        absl::StrCat(toco_flags.dump_graphviz_dir(), "/toco_AT_IMPORT.dot")));
  }

  mlir::PassManager pm(module->getContext());
  mlir::TFL::PassConfig pass_config(quant_specs);
  pass_config.emit_builtin_tflite_ops = emit_builtin_tflite_ops;
  pass_config.lower_tensor_list_ops = true;

  tensorflow::AddTFToTFLConversionPasses(pass_config, &pm);

  auto status = ConvertTFExecutorToTFLOrFlatbuffer(
      module.get(), /*export_to_mlir=*/false, emit_builtin_tflite_ops,
      emit_select_tf_ops, emit_custom_ops, /*emit_quant_adaptor_ops=*/false,
      /*lower_tensor_list_ops=*/true, quant_specs, result, &pm);

  if (toco_flags.has_dump_graphviz_dir()) {
    TF_RETURN_IF_ERROR(DumpOpGraphToFile(
        // rename once we enable the new converter feature flag.
        module.get(), absl::StrCat(toco_flags.dump_graphviz_dir(),
                                   "/toco_AFTER_TRANSFORMATIONS.dot")));
  }

  return status;
}

}  // namespace tensorflow
