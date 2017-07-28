// Fill out your copyright notice in the Description page of Project Settings.

#include "ACLData.h"
#include "ACLStatsDumpCommandlet.h"

#include "Runtime/Core/Public/HAL/FileManagerGeneric.h"
#include "Runtime/Core/Public/HAL/PlatformTime.h"
#include "Runtime/Engine/Classes/Animation/AnimCompress_Automatic.h"
#include "Runtime/Engine/Classes/Animation/Skeleton.h"

#include <acl/compression/animation_clip.h>
#include <acl/compression/skeleton_error_metric.h>
#include <acl/io/clip_reader.h>
#include <acl/math/quat_32.h>
#include <acl/math/transform_32.h>

// Commandlet example inspired by: https://github.com/ue4plugins/CommandletPlugin
// To run the commandlet, add to the commandline: "$(SolutionDir)$(ProjectName).uproject" -run=/Script/ACLData.ACLStatsDump

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

		acl::Quat_32 acl_rotation = acl::quat_cast(acl_bone.bind_rotation);
		acl::Vector4_32 acl_translation = acl::vector_cast(acl_bone.bind_translation);

		FQuat rotation(acl::quat_get_x(acl_rotation), acl::quat_get_y(acl_rotation), acl::quat_get_z(acl_rotation), acl::quat_get_w(acl_rotation));
		FVector translation(acl::vector_get_x(acl_translation), acl::vector_get_y(acl_translation), acl::vector_get_z(acl_translation));

		FTransform bone_transform(rotation, translation);
		skeleton_modifier.Add(bone, bone_transform);
	}

	// When our modifier is destroyed here, it will rebuild the skeleton
}

static void convert_clip(const acl::AnimationClip& acl_clip, const acl::RigidSkeleton& acl_skeleton, UAnimSequence* ue4_clip, USkeleton* ue4_skeleton)
{
	ue4_clip->SequenceLength = FGenericPlatformMath::Max<float>(acl_clip.get_duration(), MINIMUM_ANIMATION_LENGTH);
	ue4_clip->NumFrames = acl_clip.get_num_samples();
	//ue4_clip->CompressionScheme = auto_compressor;
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
			acl::Quat_32 acl_rotation = acl::quat_cast(bone.rotation_track.get_sample(sample_index));

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

		FName bone_name(acl_bone.name.c_str());
		ue4_clip->AddNewRawTrack(bone_name, &RawTrack);
	}

	ue4_clip->MarkRawDataAsModified();
	ue4_clip->UpdateCompressedTrackMapFromRaw();
}

static void sample_ue4_clip(const acl::RigidSkeleton& acl_skeleton, USkeleton* ue4_skeleton, UAnimSequence* ue4_clip, float sample_time, acl::Transform_32* lossy_pose_transforms)
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
		lossy_pose_transforms[bone_index] = acl::transform_set(rotation, translation);
	}
}

static float calculate_bone_max_error(const acl::AnimationClip& acl_clip, const acl::RigidSkeleton& acl_skeleton, UAnimSequence* ue4_clip, USkeleton* ue4_skeleton, uint16_t bone_index, acl::Transform_32* raw_pose_transforms, acl::Transform_32* lossy_pose_transforms, float& out_worst_sample_time)
{
	uint16_t num_bones = acl_clip.get_num_bones();
	float clip_duration = acl_clip.get_duration();
	uint32_t sample_rate = acl_clip.get_sample_rate();
	uint32_t num_samples = acl::calculate_num_samples(clip_duration, sample_rate);

	float max_error = 0.0f;
	float worst_sample_time = 0.0f;

	for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
	{
		// Sample our streams and calculate the error
		float sample_time = std::min<float>(float(sample_index) / float(sample_rate), clip_duration);

		acl_clip.sample_pose(sample_time, raw_pose_transforms, num_bones);
		sample_ue4_clip(acl_skeleton, ue4_skeleton, ue4_clip, sample_time, lossy_pose_transforms);

		float error = calculate_object_bone_error(acl_skeleton, raw_pose_transforms, lossy_pose_transforms, bone_index);
		if (error > max_error)
		{
			max_error = error;
			worst_sample_time = sample_time;
		}
	}

	out_worst_sample_time = worst_sample_time;
	return max_error;
}

static void calculate_clip_error(acl::Allocator& allocator, const acl::AnimationClip& acl_clip, const acl::RigidSkeleton& acl_skeleton, UAnimSequence* ue4_clip, USkeleton* ue4_skeleton, uint16_t& out_worst_bone, float& out_max_error, float& out_worst_sample_time)
{
	uint16_t num_bones = acl_clip.get_num_bones();

	acl::Transform_32* raw_pose_transforms = acl::allocate_type_array<acl::Transform_32>(allocator, num_bones);
	acl::Transform_32* lossy_pose_transforms = acl::allocate_type_array<acl::Transform_32>(allocator, num_bones);

	uint16_t worst_bone = acl::INVALID_BONE_INDEX;
	float max_error = 0.0f;
	float worst_sample_time = 0.0f;

	for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
	{
		float bone_worst_sample_time;
		float error = calculate_bone_max_error(acl_clip, acl_skeleton, ue4_clip, ue4_skeleton, bone_index, raw_pose_transforms, lossy_pose_transforms, bone_worst_sample_time);

		if (error > max_error)
		{
			max_error = error;
			worst_bone = bone_index;
			worst_sample_time = bone_worst_sample_time;
		}
	}

	acl::deallocate_type_array(allocator, raw_pose_transforms, num_bones);
	acl::deallocate_type_array(allocator, lossy_pose_transforms, num_bones);

	out_worst_bone = worst_bone;
	out_max_error = max_error;
	out_worst_sample_time = worst_sample_time;
}

#if 0
static FGraphEventArray ACLCompressionTask_CompletionEvents;

static bool TasksAreComplete()
{
	for (int32 Index = 0; Index < ACLCompressionTask_CompletionEvents.Num(); Index++)
	{
		if (!ACLCompressionTask_CompletionEvents[Index]->IsComplete())
			return false;
	}

	return true;
}

struct ACLSharedContext
{
	FFileManagerGeneric& file_manager;
	acl::Allocator& allocator;

	UCommandlet* commandlet;
	UAnimCompress_Automatic* auto_compressor;
	const UEnum* anim_format_enum;

	FString acl_raw_dir;
	FString ue4_stat_dir;

	ACLSharedContext(FFileManagerGeneric& file_manager, acl::Allocator& allocator)
		: file_manager(file_manager)
		, allocator(allocator)
	{}
};

class FACLCompressionTask
{

public:
	FACLCompressionTask(ACLSharedContext* context, const FString& file)
		: context(*context)
		, file(file)
	{
	}

	/** return the name of the task **/
	static const TCHAR* GetTaskName() { return TEXT("FACLCompressionTask"); }
	FORCEINLINE static TStatId GetStatId() { RETURN_QUICK_DECLARE_CYCLE_STAT(FACLCompressionTask, STATGROUP_TaskGraphTasks); }

	static ENamedThreads::Type GetDesiredThread() { return ENamedThreads::AnyThread; }
	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		FString acl_clip_path = FPaths::Combine(*context.acl_raw_dir, *file);
		FString ue4_stat_path = FPaths::Combine(*context.ue4_stat_dir, *file.Replace(TEXT(".acl.js"), TEXT("_stats.txt"), ESearchCase::CaseSensitive));

		FArchive* stat_writer = context.file_manager.CreateFileWriter(*ue4_stat_path);

		std::unique_ptr<acl::AnimationClip, acl::Deleter<acl::AnimationClip>> acl_clip;
		std::unique_ptr<acl::RigidSkeleton, acl::Deleter<acl::RigidSkeleton>> acl_skeleton;

		const TCHAR* error = read_acl_clip(context.file_manager, acl_clip_path, context.allocator, acl_clip, acl_skeleton);
		if (error == nullptr)
		{
			USkeleton* ue4_skeleton = NewObject<USkeleton>(context.commandlet, USkeleton::StaticClass());
			convert_skeleton(*acl_skeleton, ue4_skeleton);

			UAnimSequence* ue4_clip = NewObject<UAnimSequence>(context.commandlet, UAnimSequence::StaticClass());
			convert_clip(*acl_clip, *acl_skeleton, ue4_clip, ue4_skeleton);

			bool success = context.auto_compressor->Reduce(ue4_clip, false);
			if (success)
			{
				uint16_t worst_bone;
				double max_error;
				float worst_sample_time;
				calculate_clip_error(context.allocator, *acl_clip, *acl_skeleton, ue4_clip, ue4_skeleton, worst_bone, max_error, worst_sample_time);

				uint32_t acl_raw_size = acl_clip->get_total_size();
				int32 raw_size = ue4_clip->GetApproxRawSize();
				int32 compressed_size = ue4_clip->GetApproxCompressedSize();
				double compression_ratio = double(raw_size) / double(compressed_size);
				double acl_compression_ratio = double(acl_raw_size) / double(compressed_size);

				stat_writer->Logf(TEXT("Clip algorithm: %s"), *ue4_clip->CompressionScheme->GetClass()->GetName());
				stat_writer->Logf(TEXT("Clip needs skeleton: %s"), ue4_clip->CompressionScheme->bNeedsSkeleton ? TEXT("true") : TEXT("false"));
				stat_writer->Logf(TEXT("Clip raw size (bytes): %u"), raw_size);
				stat_writer->Logf(TEXT("Clip ACL raw size (bytes): %u"), acl_raw_size);
				stat_writer->Logf(TEXT("Clip compressed size (bytes): %u"), compressed_size);
				stat_writer->Logf(TEXT("Clip compression ratio: %.2f : 1"), compression_ratio);
				stat_writer->Logf(TEXT("Clip ACL compression ratio: %.2f : 1"), acl_compression_ratio);
				stat_writer->Logf(TEXT("Clip duration (s): %.5f"), ue4_clip->SequenceLength);
				stat_writer->Logf(TEXT("Clip num samples: %u"), ue4_clip->NumFrames);
				stat_writer->Logf(TEXT("Clip max error: %.5f"), max_error);
				stat_writer->Logf(TEXT("Clip worst bone: %u"), worst_bone);
				stat_writer->Logf(TEXT("Clip worst time (s): %.5f"), worst_sample_time);
				stat_writer->Logf(TEXT("Clip rotation format: %s"), *context.anim_format_enum->GetDisplayNameText(ue4_clip->CompressionScheme->RotationCompressionFormat).ToString());
				stat_writer->Logf(TEXT("Clip translation format: %s"), *context.anim_format_enum->GetDisplayNameText(ue4_clip->CompressionScheme->TranslationCompressionFormat).ToString());
				stat_writer->Logf(TEXT("Clip scale format: %s"), *context.anim_format_enum->GetDisplayNameText(ue4_clip->CompressionScheme->ScaleCompressionFormat).ToString());
			}
			else
			{
				stat_writer->Logf(TEXT("Error: failed to compress UE4 clip"));
			}
		}
		else
		{
			stat_writer->Logf(TEXT("Error: %s"), error);
		}

		stat_writer->Close();
	}

	ACLSharedContext& context;
	FString file;
};
#endif

int32 UACLStatsDumpCommandlet::Main(const FString& Params)
{
	FString acl_raw_dir(TEXT("D:\\test_animations\\carnegie-mellon-acl-raw"));
	FString ue4_stat_dir(TEXT("D:\\test_animations\\carnegie-mellon-acl-ue4-stats"));

	FFileManagerGeneric file_manager;
	TArray<FString> files;
	file_manager.FindFiles(files, *acl_raw_dir, TEXT(".acl.js"));

	acl::Allocator allocator;

	UAnimCompress_Automatic* auto_compressor = NewObject<UAnimCompress_Automatic>(this, UAnimCompress_Automatic::StaticClass());

	const UEnum* anim_format_enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("AnimationCompressionFormat"), true);

	//ACLSharedContext context(file_manager, allocator);
	//context.commandlet = this;
	//context.auto_compressor = auto_compressor;
	//context.anim_format_enum = anim_format_enum;
	//context.acl_raw_dir = acl_raw_dir;
	//context.ue4_stat_dir = ue4_stat_dir;

	for (const FString& file : files)
	{
		FString acl_clip_path = FPaths::Combine(*acl_raw_dir, *file);
		FString ue4_stat_path = FPaths::Combine(*ue4_stat_dir, *file.Replace(TEXT(".acl.js"), TEXT("_stats.txt"), ESearchCase::CaseSensitive));

		//ACLCompressionTask_CompletionEvents.Add(TGraphTask<FACLCompressionTask>::CreateTask(NULL, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(&context, file));

		FArchive* stat_writer = file_manager.CreateFileWriter(*ue4_stat_path);

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
				uint16_t worst_bone;
				float max_error;
				float worst_sample_time;
				calculate_clip_error(allocator, *acl_clip, *acl_skeleton, ue4_clip, ue4_skeleton, worst_bone, max_error, worst_sample_time);

				uint32_t acl_raw_size = acl_clip->get_total_size();
				int32 raw_size = ue4_clip->GetApproxRawSize();
				int32 compressed_size = ue4_clip->GetApproxCompressedSize();
				double compression_ratio = double(raw_size) / double(compressed_size);
				double acl_compression_ratio = double(acl_raw_size) / double(compressed_size);

				stat_writer->Logf(TEXT("Clip algorithm: %s"), *ue4_clip->CompressionScheme->GetClass()->GetName());
				stat_writer->Logf(TEXT("Clip needs skeleton: %s"), ue4_clip->CompressionScheme->bNeedsSkeleton ? TEXT("true") : TEXT("false"));
				stat_writer->Logf(TEXT("Clip raw size (bytes): %u"), raw_size);
				stat_writer->Logf(TEXT("Clip ACL raw size (bytes): %u"), acl_raw_size);
				stat_writer->Logf(TEXT("Clip compressed size (bytes): %u"), compressed_size);
				stat_writer->Logf(TEXT("Clip compression ratio: %.2f : 1"), compression_ratio);
				stat_writer->Logf(TEXT("Clip ACL compression ratio: %.2f : 1"), acl_compression_ratio);
				stat_writer->Logf(TEXT("Clip compression time (s): %.6f"), elapsed_time_sec);
				stat_writer->Logf(TEXT("Clip duration (s): %.5f"), ue4_clip->SequenceLength);
				stat_writer->Logf(TEXT("Clip num samples: %u"), ue4_clip->NumFrames);
				stat_writer->Logf(TEXT("Clip max error: %.5f"), max_error);
				stat_writer->Logf(TEXT("Clip worst bone: %u"), worst_bone);
				stat_writer->Logf(TEXT("Clip worst time (s): %.5f"), worst_sample_time);
				stat_writer->Logf(TEXT("Clip rotation format: %s"), *anim_format_enum->GetDisplayNameText(ue4_clip->CompressionScheme->RotationCompressionFormat).ToString());
				stat_writer->Logf(TEXT("Clip translation format: %s"), *anim_format_enum->GetDisplayNameText(ue4_clip->CompressionScheme->TranslationCompressionFormat).ToString());
				stat_writer->Logf(TEXT("Clip scale format: %s"), *anim_format_enum->GetDisplayNameText(ue4_clip->CompressionScheme->ScaleCompressionFormat).ToString());
			}
			else
			{
				stat_writer->Logf(TEXT("Error: failed to compress UE4 clip"));
			}
		}
		else
		{
			stat_writer->Logf(TEXT("Error: %s"), error);
		}

		stat_writer->Close();
	}

	//while (!TasksAreComplete())
	//	FPlatformProcess::Sleep(1.0f);

	return 0;
}
