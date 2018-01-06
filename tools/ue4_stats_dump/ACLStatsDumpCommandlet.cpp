// Fill out your copyright notice in the Description page of Project Settings.

#include "ACLData.h"
#include "ACLStatsDumpCommandlet.h"

#include "Runtime/Core/Public/HAL/FileManagerGeneric.h"
#include "Runtime/Core/Public/HAL/PlatformTime.h"
#include "Runtime/Engine/Classes/Animation/AnimCompress_Automatic.h"
#include "Runtime/Engine/Classes/Animation/Skeleton.h"
#include "Runtime/Engine/Public/AnimationCompression.h"

#include <acl/compression/animation_clip.h>
#include <acl/compression/skeleton_error_metric.h>
#include <acl/io/clip_reader.h>
#include <acl/sjson/sjson_writer.h>
#include <acl/math/quat_32.h>
#include <acl/math/transform_32.h>

// Commandlet example inspired by: https://github.com/ue4plugins/CommandletPlugin
// To run the commandlet, add to the commandline: "$(SolutionDir)$(ProjectName).uproject" -run=/Script/$(ProjectName).ACLStatsDump

class UE4SJSONStreamWriter final : public acl::SJSONStreamWriter
{
public:
	UE4SJSONStreamWriter(FArchive* file)
		: m_file(file)
	{}

	virtual void write(const void* buffer, size_t buffer_size) override
	{
		m_file->Serialize(const_cast<void*>(buffer), buffer_size);
	}

private:
	FArchive* m_file;
};

UACLStatsDumpCommandlet::UACLStatsDumpCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	IsClient = false;
	IsServer = false;
	IsEditor = false;
	LogToConsole = true;
	ShowErrorCount = true;
}

UACLSkeleton::UACLSkeleton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

static const TCHAR* read_acl_clip(FFileManagerGeneric& file_manager, const FString& acl_clip_path, acl::Allocator& allocator, std::unique_ptr<acl::AnimationClip, acl::Deleter<acl::AnimationClip>>& clip, std::unique_ptr<acl::RigidSkeleton, acl::Deleter<acl::RigidSkeleton>>& skeleton)
{
	FArchive* reader = file_manager.CreateFileReader(*acl_clip_path);
	int64_t size = reader->TotalSize();
	char* raw_data = (char*)GMalloc->Malloc(size);
	reader->Serialize(raw_data, size);
	reader->Close();

	acl::ClipReader clip_reader(allocator, raw_data, size);

	const TCHAR* error = nullptr;
	if (!clip_reader.read(skeleton))
		error = TEXT("Failed to read ACL RigidSkeleton from file");
	if (!clip_reader.read(clip, *skeleton))
		error = TEXT("Failed to read ACL AnimationClip from file");

	GMalloc->Free(raw_data);

	return error;
}

static void convert_skeleton(const acl::RigidSkeleton& acl_skeleton, USkeleton* ue4_skeleton)
{
	FReferenceSkeleton& ref_skeleton = const_cast<FReferenceSkeleton&>(ue4_skeleton->GetReferenceSkeleton());
	FReferenceSkeletonModifier skeleton_modifier(ref_skeleton, ue4_skeleton);

	uint16_t num_bones = acl_skeleton.get_num_bones();
	for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
	{
		const acl::RigidBone& acl_bone = acl_skeleton.get_bone(bone_index);

		FMeshBoneInfo bone;
		bone.Name = FName(acl_bone.name.c_str());
		bone.ParentIndex = acl_bone.is_root() ? INDEX_NONE : acl_bone.parent_index;
		bone.ExportName = ANSI_TO_TCHAR(acl_bone.name.c_str());

		acl::Quat_32 acl_rotation = acl::quat_cast(acl_bone.bind_transform.rotation);
		acl::Vector4_32 acl_translation = acl::vector_cast(acl_bone.bind_transform.translation);
		acl::Vector4_32 acl_scale = acl::vector_cast(acl_bone.bind_transform.scale);

		FQuat rotation(acl::quat_get_x(acl_rotation), acl::quat_get_y(acl_rotation), acl::quat_get_z(acl_rotation), acl::quat_get_w(acl_rotation));
		FVector translation(acl::vector_get_x(acl_translation), acl::vector_get_y(acl_translation), acl::vector_get_z(acl_translation));
		FVector scale(acl::vector_get_x(acl_scale), acl::vector_get_y(acl_scale), acl::vector_get_z(acl_scale));

		FTransform bone_transform(rotation, translation, scale);
		skeleton_modifier.Add(bone, bone_transform);
	}

	// When our modifier is destroyed here, it will rebuild the skeleton
}

static void convert_clip(const acl::AnimationClip& acl_clip, const acl::RigidSkeleton& acl_skeleton, UAnimSequence* ue4_clip, USkeleton* ue4_skeleton)
{
	ue4_clip->SequenceLength = FGenericPlatformMath::Max<float>(acl_clip.get_duration(), MINIMUM_ANIMATION_LENGTH);
	ue4_clip->NumFrames = acl_clip.get_num_samples();
	ue4_clip->SetSkeleton(ue4_skeleton);

	uint16_t num_bones = acl_skeleton.get_num_bones();
	for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
	{
		const acl::RigidBone& acl_bone = acl_skeleton.get_bone(bone_index);
		const acl::AnimatedBone& bone = acl_clip.get_animated_bone(bone_index);

		FRawAnimSequenceTrack RawTrack;
		RawTrack.PosKeys.Empty();
		RawTrack.RotKeys.Empty();
		RawTrack.ScaleKeys.Empty();

		uint32_t num_samples = bone.rotation_track.get_num_samples();
		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			acl::Quat_32 acl_rotation = acl::quat_normalize(acl::quat_cast(bone.rotation_track.get_sample(sample_index)));

			FQuat rotation(acl::quat_get_x(acl_rotation), acl::quat_get_y(acl_rotation), acl::quat_get_z(acl_rotation), acl::quat_get_w(acl_rotation));
			RawTrack.RotKeys.Add(rotation);
		}

		num_samples = bone.translation_track.get_num_samples();
		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			acl::Vector4_32 acl_translation = acl::vector_cast(bone.translation_track.get_sample(sample_index));

			FVector translation(acl::vector_get_x(acl_translation), acl::vector_get_y(acl_translation), acl::vector_get_z(acl_translation));
			RawTrack.PosKeys.Add(translation);
		}

		num_samples = bone.scale_track.get_num_samples();
		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			acl::Vector4_32 acl_scale = acl::vector_cast(bone.scale_track.get_sample(sample_index));

			FVector scale(acl::vector_get_x(acl_scale), acl::vector_get_y(acl_scale), acl::vector_get_z(acl_scale));
			RawTrack.ScaleKeys.Add(scale);
		}

		FName bone_name(acl_bone.name.c_str());
		ue4_clip->AddNewRawTrack(bone_name, &RawTrack);
	}

	ue4_clip->MarkRawDataAsModified();
	ue4_clip->UpdateCompressedTrackMapFromRaw();
	ue4_clip->PostProcessSequence();
}

static void sample_ue4_clip(const acl::RigidSkeleton& acl_skeleton, USkeleton* ue4_skeleton, const UAnimSequence* ue4_clip, float sample_time, acl::Transform_32* lossy_pose_transforms)
{
	const FReferenceSkeleton& ref_skeleton = ue4_skeleton->GetReferenceSkeleton();

	uint16_t num_bones = acl_skeleton.get_num_bones();
	for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
	{
		const acl::RigidBone& acl_bone = acl_skeleton.get_bone(bone_index);
		FName bone_name(acl_bone.name.c_str());
		int32 BoneTreeIndex = ref_skeleton.FindBoneIndex(bone_name);
		int32 BoneTrackIndex = ue4_skeleton->GetAnimationTrackIndex(BoneTreeIndex, ue4_clip, false);

		FTransform BoneAtom;
		ue4_clip->GetBoneTransform(BoneAtom, BoneTrackIndex, sample_time, false);

		acl::Quat_32 rotation = acl::quat_set(BoneAtom.GetRotation().X, BoneAtom.GetRotation().Y, BoneAtom.GetRotation().Z, BoneAtom.GetRotation().W);
		acl::Vector4_32 translation = acl::vector_set(BoneAtom.GetTranslation().X, BoneAtom.GetTranslation().Y, BoneAtom.GetTranslation().Z);
		acl::Vector4_32 scale = acl::vector_set(BoneAtom.GetScale3D().X, BoneAtom.GetScale3D().Y, BoneAtom.GetScale3D().Z);
		lossy_pose_transforms[bone_index] = acl::transform_set(rotation, translation, scale);
	}
}

static bool ue4_clip_has_scale(const UAnimSequence* ue4_clip)
{
	const TArray<FRawAnimSequenceTrack>& tracks = ue4_clip->GetRawAnimationData();
	for (const FRawAnimSequenceTrack& track : tracks)
	{
		if (track.ScaleKeys.Num() != 0)
			return true;
	}

	return false;
}

static void calculate_clip_error(acl::Allocator& allocator, const acl::AnimationClip& acl_clip, const acl::RigidSkeleton& acl_skeleton, const UAnimSequence* ue4_clip, USkeleton* ue4_skeleton, uint16_t& out_worst_bone, float& out_max_error, float& out_worst_sample_time)
{
	using namespace acl;

	uint16_t num_bones = acl_clip.get_num_bones();
	float clip_duration = acl_clip.get_duration();
	float sample_rate = float(acl_clip.get_sample_rate());
	uint32_t num_samples = calculate_num_samples(clip_duration, sample_rate);
	bool has_scale = ue4_clip_has_scale(ue4_clip);

	Transform_32* raw_pose_transforms = allocate_type_array<Transform_32>(allocator, num_bones);
	Transform_32* lossy_pose_transforms = allocate_type_array<Transform_32>(allocator, num_bones);

	uint16_t worst_bone = acl::INVALID_BONE_INDEX;
	float max_error = 0.0f;
	float worst_sample_time = 0.0f;

	TransformErrorMetric error_metric;

	for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
	{
		// Sample our streams and calculate the error
		float sample_time = min(float(sample_index) / sample_rate, clip_duration);

		acl_clip.sample_pose(sample_time, raw_pose_transforms, num_bones);
		sample_ue4_clip(acl_skeleton, ue4_skeleton, ue4_clip, sample_time, lossy_pose_transforms);

		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			float error;
			if (has_scale)
				error = error_metric.calculate_object_bone_error(acl_skeleton, raw_pose_transforms, lossy_pose_transforms, bone_index);
			else
				error = error_metric.calculate_object_bone_error_no_scale(acl_skeleton, raw_pose_transforms, lossy_pose_transforms, bone_index);

			if (error > max_error)
			{
				max_error = error;
				worst_bone = bone_index;
				worst_sample_time = sample_time;
			}
		}
	}

	deallocate_type_array(allocator, raw_pose_transforms, num_bones);
	deallocate_type_array(allocator, lossy_pose_transforms, num_bones);

	out_worst_bone = worst_bone;
	out_max_error = max_error;
	out_worst_sample_time = worst_sample_time;
}

static void dump_clip_detailed_error(acl::Allocator& allocator, const acl::AnimationClip& acl_clip, const acl::RigidSkeleton& acl_skeleton, UAnimSequence* ue4_clip, USkeleton* ue4_skeleton, acl::SJSONObjectWriter& writer)
{
	using namespace acl;

	uint16_t num_bones = acl_clip.get_num_bones();
	float clip_duration = acl_clip.get_duration();
	float sample_rate = float(acl_clip.get_sample_rate());
	uint32_t num_samples = calculate_num_samples(clip_duration, sample_rate);
	bool has_scale = ue4_clip_has_scale(ue4_clip);

	Transform_32* raw_pose_transforms = allocate_type_array<Transform_32>(allocator, num_bones);
	Transform_32* lossy_pose_transforms = allocate_type_array<Transform_32>(allocator, num_bones);

	TransformErrorMetric error_metric;

	writer["error_per_frame_and_bone"] = [&](SJSONArrayWriter& writer)
	{
		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			// Sample our streams and calculate the error
			float sample_time = min(float(sample_index) / sample_rate, clip_duration);

			acl_clip.sample_pose(sample_time, raw_pose_transforms, num_bones);
			sample_ue4_clip(acl_skeleton, ue4_skeleton, ue4_clip, sample_time, lossy_pose_transforms);

			writer.push_newline();
			writer.push_array([&](SJSONArrayWriter& writer)
			{
				for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
				{
					float error;
					if (has_scale)
						error = error_metric.calculate_object_bone_error(acl_skeleton, raw_pose_transforms, lossy_pose_transforms, bone_index);
					else
						error = error_metric.calculate_object_bone_error_no_scale(acl_skeleton, raw_pose_transforms, lossy_pose_transforms, bone_index);

					writer.push_value(error);
				}
			});
		}
	};

	deallocate_type_array(allocator, raw_pose_transforms, num_bones);
	deallocate_type_array(allocator, lossy_pose_transforms, num_bones);
}

int32 UACLStatsDumpCommandlet::Main(const FString& Params)
{
	FString acl_raw_dir(TEXT("D:\\test_animations\\carnegie-mellon-acl-raw"));
	FString ue4_stat_dir(TEXT("D:\\test_animations\\carnegie-mellon-acl-ue4-stats"));
	const bool exhaustive_dump = true;
	const float master_tolerance = 0.1f;

	FFileManagerGeneric file_manager;
	TArray<FString> files;
	file_manager.FindFiles(files, *acl_raw_dir, TEXT(".acl.sjson"));

	acl::Allocator allocator;

	UAnimCompress_Automatic* auto_compressor = NewObject<UAnimCompress_Automatic>(this, UAnimCompress_Automatic::StaticClass());
	auto_compressor->MaxEndEffectorError = master_tolerance;
	auto_compressor->bAutoReplaceIfExistingErrorTooGreat = true;

	const UEnum* anim_format_enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("AnimationCompressionFormat"), true);

	for (const FString& file : files)
	{
		FString acl_clip_path = FPaths::Combine(*acl_raw_dir, *file);
		FString ue4_stat_path = FPaths::Combine(*ue4_stat_dir, *file.Replace(TEXT(".acl.sjson"), TEXT("_stats.sjson"), ESearchCase::CaseSensitive));

		if (file_manager.FileExists(*ue4_stat_path))
			continue;

		FArchive* stat_writer = file_manager.CreateFileWriter(*ue4_stat_path);
		UE4SJSONStreamWriter stream_writer(stat_writer);
		acl::SJSONWriter writer(stream_writer);

		std::unique_ptr<acl::AnimationClip, acl::Deleter<acl::AnimationClip>> acl_clip;
		std::unique_ptr<acl::RigidSkeleton, acl::Deleter<acl::RigidSkeleton>> acl_skeleton;

		const TCHAR* error = read_acl_clip(file_manager, acl_clip_path, allocator, acl_clip, acl_skeleton);
		if (error == nullptr)
		{
			USkeleton* ue4_skeleton = NewObject<USkeleton>(this, USkeleton::StaticClass());
			convert_skeleton(*acl_skeleton, ue4_skeleton);

			UAnimSequence* ue4_clip = NewObject<UAnimSequence>(this, UAnimSequence::StaticClass());
			convert_clip(*acl_clip, *acl_skeleton, ue4_clip, ue4_skeleton);

			uint64_t read_start_time_cycles = FPlatformTime::Cycles64();

			bool success = auto_compressor->Reduce(ue4_clip, false);

			uint64_t read_end_time_cycles = FPlatformTime::Cycles64();

			uint64_t elapsed_cycles = read_end_time_cycles - read_start_time_cycles;
			double elapsed_time_sec = FPlatformTime::ToSeconds64(elapsed_cycles);

			if (success)
			{
				TArray<FBoneData> ue4_bone_data;
				FAnimationUtils::BuildSkeletonMetaData(ue4_skeleton, ue4_bone_data);

				AnimationErrorStats ue4_error_stats;
				FAnimationUtils::ComputeCompressionError(ue4_clip, ue4_bone_data, ue4_error_stats);

				uint16_t worst_bone;
				float max_error;
				float worst_sample_time;
				calculate_clip_error(allocator, *acl_clip, *acl_skeleton, ue4_clip, ue4_skeleton, worst_bone, max_error, worst_sample_time);

				uint32_t acl_raw_size = acl_clip->get_raw_size();
				int32 raw_size = ue4_clip->GetApproxRawSize();
				int32 compressed_size = ue4_clip->GetApproxCompressedSize();
				double compression_ratio = double(raw_size) / double(compressed_size);
				double acl_compression_ratio = double(acl_raw_size) / double(compressed_size);

				writer["algorithm_name"] = TCHAR_TO_ANSI(*ue4_clip->CompressionScheme->GetClass()->GetName());
				writer["ue4_raw_size"] = raw_size;
				writer["acl_raw_size"] = acl_raw_size;
				writer["compressed_size"] = compressed_size;
				writer["ue4_compression_ratio"] = compression_ratio;
				writer["acl_compression_ratio"] = acl_compression_ratio;
				writer["compression_time"] = elapsed_time_sec;
				writer["duration"] = ue4_clip->SequenceLength;
				writer["num_samples"] = ue4_clip->NumFrames;
				writer["ue4_max_error"] = ue4_error_stats.MaxError;
				writer["ue4_avg_error"] = ue4_error_stats.AverageError;
				writer["ue4_worst_bone"] = ue4_error_stats.MaxErrorBone;
				writer["ue4_worst_time"] = ue4_error_stats.MaxErrorTime;
				writer["acl_max_error"] = max_error;
				writer["acl_worst_bone"] = worst_bone;
				writer["acl_worst_time"] = worst_sample_time;
				writer["rotation_format"] = TCHAR_TO_ANSI(*anim_format_enum->GetDisplayNameText(ue4_clip->CompressionScheme->RotationCompressionFormat).ToString());
				writer["translation_format"] = TCHAR_TO_ANSI(*anim_format_enum->GetDisplayNameText(ue4_clip->CompressionScheme->TranslationCompressionFormat).ToString());
				writer["scale_format"] = TCHAR_TO_ANSI(*anim_format_enum->GetDisplayNameText(ue4_clip->CompressionScheme->ScaleCompressionFormat).ToString());

				if (exhaustive_dump)
					dump_clip_detailed_error(allocator, *acl_clip, *acl_skeleton, ue4_clip, ue4_skeleton, writer);
			}
			else
			{
				writer["error"] = "failed to compress UE4 clip";
			}

			ue4_clip->RecycleAnimSequence();
		}
		else
		{
			writer["error"] = TCHAR_TO_ANSI(error);
		}

		stat_writer->Close();
	}

	return 0;
}
