// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_phishing_model_optimization_guide.h"

#include <stdint.h>
#include <memory>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/safe_browsing/core/common/fbs/client_model_generated.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

namespace {

// Command-line flag that can be used to override the current CSD model. Must be
// provided with an absolute path.
const char kOverrideCsdModelFlag[] = "csd-model-override-path";

void ReturnModelOverrideFailure(
    base::OnceCallback<void(std::pair<std::string, base::File>)> callback) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                std::make_pair(std::string(), base::File())));
}

void ReadOverridenModel(
    base::FilePath path,
    base::OnceCallback<void(std::pair<std::string, base::File>)> callback) {
  if (path.empty()) {
    VLOG(2) << "Failed to override model. Path is empty. Path is " << path;
    ReturnModelOverrideFailure(std::move(callback));
    return;
  }

  std::string contents;
  if (!base::ReadFileToString(path.AppendASCII("client_model.pb"), &contents)) {
    VLOG(2) << "Failed to override model. Could not read model data.";
    ReturnModelOverrideFailure(std::move(callback));
    return;
  }

  base::File tflite_model(path.AppendASCII("visual_model.tflite"),
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
  // `tflite_model` is allowed to be invalid, when testing a DOM-only model.

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     std::make_pair(contents, std::move(tflite_model))));
}

// Load the model file at the provided file path.
std::pair<std::string, base::File> LoadModelAndVisualTfLiteFile(
    const base::FilePath& model_file_path,
    base::flat_set<base::FilePath> additional_files) {
  if (!base::PathExists(model_file_path)) {
    VLOG(0) << "Model path does not exist. Returning empty pair. Given path is "
            << model_file_path;
    return std::pair<std::string, base::File>();
  }

  // The only additional file we require and expect is the visual tf lite file
  if (additional_files.size() != 1) {
    VLOG(2) << "Did not receive just one additional file when expected. "
               "Actual size: "
            << additional_files.size();
    return std::pair<std::string, base::File>();
  }

  absl::optional<base::FilePath> visual_tflite_path = absl::nullopt;

  for (const base::FilePath& path : additional_files) {
    // There should only be one loop after above check
    DCHECK(path.IsAbsolute());
    visual_tflite_path = path;
  }

  base::File model(model_file_path,
                   base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::File tf_lite(*visual_tflite_path,
                     base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!model.IsValid() || !tf_lite.IsValid()) {
    VLOG(2) << "Failed to override the model and/or tf_lite file.";
  }

  // Convert model to string
  std::string model_data;
  if (!base::ReadFileToString(model_file_path, &model_data)) {
    VLOG(2) << "Failed to override model. Could not read model data.";
    return std::pair<std::string, base::File>();
  }

  return std::make_pair(std::string(model_data.begin(), model_data.end()),
                        std::move(tf_lite));
}

// Close the provided model file.
void CloseModelFile(base::File model_file) {
  if (!model_file.IsValid()) {
    return;
  }
  model_file.Close();
}

}  // namespace

// --- ClientSidePhishingModelOptimizationGuide methods ---

ClientSidePhishingModelOptimizationGuide::
    ClientSidePhishingModelOptimizationGuide(
        optimization_guide::OptimizationGuideModelProvider* opt_guide,
        const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
    : opt_guide_(opt_guide),
      background_task_runner_(background_task_runner),
      beginning_time_(base::TimeTicks::Now()) {
  opt_guide_->AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING,
      /*model_metadata=*/absl::nullopt, this);
}

void ClientSidePhishingModelOptimizationGuide::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    const optimization_guide::ModelInfo& model_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (optimization_target !=
      optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING) {
    return;
  }
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LoadModelAndVisualTfLiteFile,
                     model_info.GetModelFilePath(),
                     model_info.GetAdditionalFiles()),
      base::BindOnce(&ClientSidePhishingModelOptimizationGuide::
                         OnModelAndVisualTfLiteFileLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ClientSidePhishingModelOptimizationGuide::OnModelAndVisualTfLiteFileLoaded(
    std::pair<std::string, base::File> model_and_tflite) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (visual_tflite_model_) {
    // If the visual tf lite file is already loaded, it should be closed on a
    // background thread.
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CloseModelFile, std::move(*visual_tflite_model_)));
  }

  std::string model_str = std::move(model_and_tflite.first);
  base::File visual_tflite_model = std::move(model_and_tflite.second);

  bool model_valid = false;
  int model_version_field = 0;

  bool tflite_valid = visual_tflite_model.IsValid();
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          kOverrideCsdModelFlag) &&
      !model_str.empty()) {
    model_type_ = CSDModelTypeOptimizationGuide::kNone;
    flatbuffers::Verifier verifier(
        reinterpret_cast<const uint8_t*>(model_str.data()), model_str.length());
    model_valid = flat::VerifyClientSideModelBuffer(verifier);
    if (model_valid) {
      mapped_region_ =
          base::ReadOnlySharedMemoryRegion::Create(model_str.length());
      if (mapped_region_.IsValid()) {
        model_type_ = CSDModelTypeOptimizationGuide::kFlatbuffer;
        model_version_field =
            flat::GetClientSideModel(model_str.data())->version();
        memcpy(mapped_region_.mapping.memory(), model_str.data(),
               model_str.length());

        const flat::ClientSideModel* flatbuffer_model =
            flat::GetClientSideModel(mapped_region_.mapping.memory());

        if (!VerifyCSDFlatBufferIndicesAndFields(flatbuffer_model)) {
          VLOG(0) << "Failed to verify CSD Flatbuffer indices and fields";
        } else {
          if (tflite_valid) {
            thresholds_.clear();  // Clear the previous model's thresholds
                                  // before adding on the new ones
            for (const flat::TfLiteModelMetadata_::Threshold* flat_threshold :
                 *(flatbuffer_model->tflite_metadata()->thresholds())) {
              TfLiteModelMetadata::Threshold threshold;
              threshold.set_label(flat_threshold->label()->str());
              threshold.set_threshold(flat_threshold->threshold());
              threshold.set_esb_threshold(flat_threshold->esb_threshold() > 0
                                              ? flat_threshold->esb_threshold()
                                              : flat_threshold->threshold());
              thresholds_[flat_threshold->label()->str()] = threshold;
            }
          }
        }
      } else {
        model_valid = false;
      }
      base::UmaHistogramBoolean("SBClientPhishing.FlatBufferMappedRegionValid",
                                mapped_region_.IsValid());
    } else {
      VLOG(2) << "Failed to validate flatbuffer model";
    }
  }

  base::UmaHistogramBoolean("SBClientPhishing.ModelDynamicUpdateSuccess",
                            model_valid);

  if (model_valid) {
    // At time of writing, versions go up to 25. We set a max version of 100
    // to give some room.
    const int kMaxVersion = 100;
    base::UmaHistogramExactLinear("SBClientPhishing.ModelDynamicUpdateVersion",
                                  model_version_field, kMaxVersion + 1);
  }

  if (tflite_valid) {
    visual_tflite_model_ = std::move(visual_tflite_model);
  }

  if (model_valid && tflite_valid) {
    base::UmaHistogramMediumTimes(
        "SBClientPhishing.OptimizationGuide.ModelFetchTime",
        base::TimeTicks::Now() - beginning_time_);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ClientSidePhishingModelOptimizationGuide::NotifyCallbacksOnUI,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

ClientSidePhishingModelOptimizationGuide::
    ~ClientSidePhishingModelOptimizationGuide() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  opt_guide_->RemoveObserverForOptimizationTargetModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING,
      this);
  opt_guide_ = nullptr;

  if (visual_tflite_model_) {
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CloseModelFile, std::move(*visual_tflite_model_)));
  }
}

base::CallbackListSubscription
ClientSidePhishingModelOptimizationGuide::RegisterCallback(
    base::RepeatingCallback<void()> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return callbacks_.Add(std::move(callback));
}

bool ClientSidePhishingModelOptimizationGuide::IsEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return (model_type_ == CSDModelTypeOptimizationGuide::kFlatbuffer &&
          mapped_region_.IsValid() && visual_tflite_model_ &&
          visual_tflite_model_->IsValid()) ||
         (model_type_ == CSDModelTypeOptimizationGuide::kProtobuf &&
          !model_str_.empty());
}

// static
bool ClientSidePhishingModelOptimizationGuide::
    VerifyCSDFlatBufferIndicesAndFields(const flat::ClientSideModel* model) {
  const flatbuffers::Vector<flatbuffers::Offset<flat::Hash>>* hashes =
      model->hashes();
  if (!hashes) {
    return false;
  }

  const flatbuffers::Vector<flatbuffers::Offset<flat::ClientSideModel_::Rule>>*
      rules = model->rule();
  if (!rules) {
    return false;
  }
  for (const flat::ClientSideModel_::Rule* rule : *model->rule()) {
    if (!rule || !rule->feature()) {
      return false;
    }
    for (int32_t feature : *rule->feature()) {
      if (feature < 0 || feature >= static_cast<int32_t>(hashes->size())) {
        return false;
      }
    }
  }

  const flatbuffers::Vector<int32_t>* page_terms = model->page_term();
  if (!page_terms) {
    return false;
  }
  for (int32_t page_term_idx : *page_terms) {
    if (page_term_idx < 0 ||
        page_term_idx >= static_cast<int32_t>(hashes->size())) {
      return false;
    }
  }

  const flatbuffers::Vector<uint32_t>* page_words = model->page_word();
  if (!page_words) {
    return false;
  }

  const flat::TfLiteModelMetadata* metadata = model->tflite_metadata();
  if (!metadata) {
    return false;
  }
  const flatbuffers::Vector<
      flatbuffers::Offset<flat::TfLiteModelMetadata_::Threshold>>* thresholds =
      metadata->thresholds();
  if (!thresholds) {
    return false;
  }
  for (const flat::TfLiteModelMetadata_::Threshold* threshold : *thresholds) {
    if (!threshold || !threshold->label()) {
      return false;
    }
  }

  return true;
}

const base::flat_map<std::string, TfLiteModelMetadata::Threshold>&
ClientSidePhishingModelOptimizationGuide::GetVisualTfLiteModelThresholds()
    const {
  return thresholds_;
}

const std::string& ClientSidePhishingModelOptimizationGuide::GetModelStr()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(model_type_ != CSDModelTypeOptimizationGuide::kFlatbuffer);
  return model_str_;
}

const base::File&
ClientSidePhishingModelOptimizationGuide::GetVisualTfLiteModel() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(visual_tflite_model_ && visual_tflite_model_->IsValid());
  return *visual_tflite_model_;
}

CSDModelTypeOptimizationGuide
ClientSidePhishingModelOptimizationGuide::GetModelType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return model_type_;
}

base::ReadOnlySharedMemoryRegion
ClientSidePhishingModelOptimizationGuide::GetModelSharedMemoryRegion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return mapped_region_.region.Duplicate();
}

void ClientSidePhishingModelOptimizationGuide::SetModelStringForTesting(
    const std::string& model_str,
    base::File visual_tflite_model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool model_valid = false;
  int model_version_field = 0;

  bool tflite_valid = visual_tflite_model.IsValid();

  // TODO (andysjlim): Move to a helper function once flatbuffer feature is
  // removed
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          kOverrideCsdModelFlag) &&
      !model_str.empty()) {
    model_type_ = CSDModelTypeOptimizationGuide::kNone;
    if (base::FeatureList::IsEnabled(kClientSideDetectionModelIsFlatBuffer)) {
      flatbuffers::Verifier verifier(
          reinterpret_cast<const uint8_t*>(model_str.data()),
          model_str.length());
      model_valid = flat::VerifyClientSideModelBuffer(verifier);
      if (model_valid) {
        mapped_region_ =
            base::ReadOnlySharedMemoryRegion::Create(model_str.length());
        if (mapped_region_.IsValid()) {
          model_type_ = CSDModelTypeOptimizationGuide::kFlatbuffer;
          model_version_field =
              flat::GetClientSideModel(model_str.data())->version();
          memcpy(mapped_region_.mapping.memory(), model_str.data(),
                 model_str.length());
        } else {
          model_valid = false;
        }
        base::UmaHistogramBoolean(
            "SBClientPhishing.FlatBufferMappedRegionValid",
            mapped_region_.IsValid());
      }
    } else {
      ClientSideModel model_proto;
      model_valid = model_proto.ParseFromString(model_str);
      if (model_valid) {
        model_type_ = CSDModelTypeOptimizationGuide::kProtobuf;
        model_version_field = model_proto.version();
        model_str_ = model_str;
      }
    }

    base::UmaHistogramBoolean("SBClientPhishing.ModelDynamicUpdateSuccess",
                              model_valid);

    if (model_valid) {
      // At time of writing, versions go up to 25. We set a max version of 100
      // to give some room.
      const int kMaxVersion = 100;
      base::UmaHistogramExactLinear(
          "SBClientPhishing.ModelDynamicUpdateVersion", model_version_field,
          kMaxVersion + 1);
    }

    if (tflite_valid) {
      visual_tflite_model_ = std::move(visual_tflite_model);
    }
  }

  if (model_valid || tflite_valid) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ClientSidePhishingModelOptimizationGuide::NotifyCallbacksOnUI,
            base::Unretained(this)));
  }
}

void ClientSidePhishingModelOptimizationGuide::NotifyCallbacksOnUI() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  callbacks_.Notify();
}

void ClientSidePhishingModelOptimizationGuide::SetModelStrForTesting(
    const std::string& model_str) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  model_str_ = model_str;
}

void ClientSidePhishingModelOptimizationGuide::SetVisualTfLiteModelForTesting(
    base::File file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  visual_tflite_model_ = std::move(file);
}

void ClientSidePhishingModelOptimizationGuide::SetModelTypeForTesting(
    CSDModelTypeOptimizationGuide model_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  model_type_ = model_type;
}

void ClientSidePhishingModelOptimizationGuide::ClearMappedRegionForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mapped_region_.mapping = base::WritableSharedMemoryMapping();
  mapped_region_.region = base::ReadOnlySharedMemoryRegion();
}

void* ClientSidePhishingModelOptimizationGuide::
    GetFlatBufferMemoryAddressForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return mapped_region_.mapping.memory();
}

void ClientSidePhishingModelOptimizationGuide::
    NotifyCallbacksOfUpdateForTesting() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ClientSidePhishingModelOptimizationGuide::NotifyCallbacksOnUI,
          base::Unretained(this)));
}

// This function is used for testing in client_side_phishing_model_unittest
void ClientSidePhishingModelOptimizationGuide::MaybeOverrideModel() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kOverrideCsdModelFlag)) {
    base::FilePath overriden_model_directory =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            kOverrideCsdModelFlag);
    CSDModelTypeOptimizationGuide model_type =
        base::FeatureList::IsEnabled(kClientSideDetectionModelIsFlatBuffer)
            ? CSDModelTypeOptimizationGuide::kFlatbuffer
            : CSDModelTypeOptimizationGuide::kProtobuf;
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(
            &ReadOverridenModel, overriden_model_directory,
            base::BindOnce(&ClientSidePhishingModelOptimizationGuide::
                               OnGetOverridenModelData,
                           base::Unretained(this), model_type)));
  }
}

// This function is used for testing in client_side_phishing_model_unittest
void ClientSidePhishingModelOptimizationGuide::OnGetOverridenModelData(
    CSDModelTypeOptimizationGuide model_type,
    std::pair<std::string, base::File> model_and_tflite) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string& model_data = model_and_tflite.first;
  base::File tflite_model = std::move(model_and_tflite.second);
  if (model_data.empty()) {
    VLOG(2) << "Overriden model data is empty";
    return;
  }

  switch (model_type) {
    case CSDModelTypeOptimizationGuide::kProtobuf: {
      std::unique_ptr<ClientSideModel> model =
          std::make_unique<ClientSideModel>();
      if (!model->ParseFromArray(model_data.data(), model_data.size())) {
        VLOG(2) << "Overriden model data is not a valid ClientSideModel proto";
        return;
      }
      model_type_ = model_type;
      model_str_ = model_data;
      break;
    }
    case CSDModelTypeOptimizationGuide::kFlatbuffer: {
      flatbuffers::Verifier verifier(
          reinterpret_cast<const uint8_t*>(model_data.data()),
          model_data.length());
      if (!flat::VerifyClientSideModelBuffer(verifier)) {
        VLOG(2)
            << "Overriden model data is not a valid ClientSideModel flatbuffer";
        return;
      }
      mapped_region_ =
          base::ReadOnlySharedMemoryRegion::Create(model_data.length());
      if (!mapped_region_.IsValid()) {
        VLOG(2) << "Could not create shared memory region for flatbuffer";
        return;
      }
      memcpy(mapped_region_.mapping.memory(), model_data.data(),
             model_data.length());
      model_type_ = model_type;
      break;
    }
    case CSDModelTypeOptimizationGuide::kNone:
      VLOG(2) << "Model type should have been either proto or flatbuffer";
      return;
  }

  if (tflite_model.IsValid()) {
    visual_tflite_model_ = std::move(tflite_model);
  }

  VLOG(0) << "Model overridden successfully";

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ClientSidePhishingModelOptimizationGuide::NotifyCallbacksOnUI,
          weak_ptr_factory_.GetWeakPtr()));
}

// For browser unit testing in client_side_detection_service_browsertest
void ClientSidePhishingModelOptimizationGuide::
    SetModelAndVisualTfLiteForTesting(
        const base::FilePath& model_file_path,
        const base::FilePath& visual_tf_lite_model_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::flat_set<base::FilePath> additional_files;
  additional_files.insert(visual_tf_lite_model_path);
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LoadModelAndVisualTfLiteFile, model_file_path,
                     additional_files),
      base::BindOnce(&ClientSidePhishingModelOptimizationGuide::
                         OnModelAndVisualTfLiteFileLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace safe_browsing
