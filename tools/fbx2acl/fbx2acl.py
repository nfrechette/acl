# For this script to work, you must first install the FBX SKD Python bindings
# Make sure your python version is supported by the bindings as well

import sys
import zipfile
import os
import io
from collections import namedtuple

FBXNode = namedtuple('FBXNode', 'name parent node')
ACLClip = namedtuple('ACLClip', 'name num_samples sample_rate error_threshold duration')
ACLBone = namedtuple('ACLBone', 'name parent vtx_distance obj_space_mtx bind_rotation bind_translation bind_scale')
ACLTrack = namedtuple('ACLTrack', 'name rotations translations scales')

ACL_FILE_FORMAT_VERSION = 1

def print_animation_stacks(scene):
	for i in range(scene.GetSrcObjectCount(FbxAnimStack.ClassId)):
		print('  "' + scene.GetSrcObject(FbxAnimStack.ClassId, i).GetName() + '"')

def get_animation_stack(scene, anim_stack_name):
	num_stacks = scene.GetSrcObjectCount(FbxAnimStack.ClassId)
	if num_stacks == 1:
		anim_stack = scene.GetSrcObject(FbxAnimStack.ClassId, 0)
		if len(anim_stack_name) > 0 and anim_stack_name != anim_stack.GetName():
			print('There is one animation stack, but it\'s called "' + anim_stack.GetName() + '".  Consider omitting the -stack option.')
			sys.exit(1)
	else:
		if len(anim_stack_name) == 0:
			print('You must provide the -stack="<animation stack name>" option for one of these:')
			print_animation_stacks(scene)
			sys.exit(1)
		
		found = False
		for i in range(scene.GetSrcObjectCount(FbxAnimStack.ClassId)):
			anim_stack = scene.GetSrcObject(FbxAnimStack.ClassId, i)
			if anim_stack.GetName() == anim_stack_name:
				found = True
				break
	
		if not found:
			print('Could not find the animation stack named "' + anim_stack_name + '"')
			print('Choose one of the following instead:')
			print_animation_stacks(scene)
			sys.exit(1)
	
	return anim_stack

def parse_clip(scene, anim_stack):
	clip_name = anim_stack.GetName()
	timespan = anim_stack.GetLocalTimeSpan()
	sample_rate = int(FbxTime.GetFrameRate(scene.GetGlobalSettings().GetTimeMode()) + 0.5)
	duration = timespan.GetDuration().GetSecondDouble()
	num_samples = int((duration * sample_rate) + 0.5) + 1
	error_threshold = 0.01

	return ACLClip(clip_name, num_samples, sample_rate, error_threshold, duration)

def parse_hierarchy(scene):
	nodes = []

	root_node = scene.GetRootNode()
	nodes.append(FBXNode(root_node.GetName(), "", root_node))

	for i in range(root_node.GetChildCount()):
		parse_hierarchy_node(root_node, root_node.GetChild(i), nodes)

	return nodes

def parse_hierarchy_node(parent_node, node, nodes):
	type = (node.GetNodeAttribute().GetAttributeType())
	if type != FbxNodeAttribute.eSkeleton:
		print('Ignoring node ' + node.GetName())
		return

	nodes.append(FBXNode(node.GetName(), parent_node.GetName(), node))

	for i in range(node.GetChildCount()):
		parse_hierarchy_node(node, node.GetChild(i), nodes)

def vector3_to_array(vec):
	return [ vec[0], vec[1], vec[2] ]

def quaternion_to_array(vec):
	return [ vec[0], vec[1], vec[2], vec[3] ]

def parse_bind_pose(scene, nodes):
	bones = []

	vtx_distance = 3.0

	translation = FbxVector4()
	rotation = FbxQuaternion()
	shear = FbxVector4()
	scale = FbxVector4()

	for pose_idx in range(scene.GetPoseCount()):
		pose = scene.GetPose(pose_idx)

		if not pose.IsBindPose():
			continue

		for bone_idx in range(pose.GetCount()):
			bone_name = pose.GetNodeName(bone_idx).GetCurrentName()

			try:
				bone_node = next(x for x in nodes if x.name == bone_name)
			except StopIteration:
				continue

			matrix = pose.GetMatrix(bone_idx)

			if bone_node.parent == "" or bone_node.parent == nodes[0].name:
				parent_name = ""
				local_space_mtx = matrix
			else:
				parent_name = bone_node.parent
				parent_bone = next(x for x in bones if x.name == parent_name)
				local_space_mtx = matrix * parent_bone.obj_space_mtx.Inverse()

			# Convert from FBX types to float arrays
			local_space_mtx.GetElements(translation, rotation, shear, scale)
			rotation_array = quaternion_to_array(rotation)
			translation_array = vector3_to_array(translation)
			scale_array = vector3_to_array(scale)

			bone = ACLBone(bone_name, parent_name, vtx_distance, matrix, rotation_array, translation_array, scale_array)
			bones.append(bone)

		# Stop after we parsed the bind pose
		break

	return bones

def is_key_default(key, default_value, error_threshold = 0.000001):
	for channel_index in range(len(default_value)):
		if abs(key[channel_index] - default_value[channel_index]) > error_threshold:
			# Something isn't equal to our default value
			return False

	# Everything is equal, we are a default key
	return True

def is_track_default(track, default_value, error_threshold = 0.000001):
	if len(track) == 0:
		# Empty track is considered a default track
		return True

	num_channels = len(default_value)

	for key in track:
		if not is_key_default(key, default_value, error_threshold):
			return False

	# Everything is equal, we are a default track
	return True

def parse_tracks(scene, clip, bones, nodes):
	tracks = []

	root_node = scene.GetRootNode()
	anim_evaluator = scene.GetAnimationEvaluator()

	time = FbxTime()
	frame_duration = 1.0 / clip.sample_rate
	default_rotation = [ 0.0, 0.0, 0.0, 1.0 ]
	default_translation = [ 0.0, 0.0, 0.0 ]
	default_scale = [ 1.0, 1.0, 1.0 ]

	for bone in bones:
		bone_node = next(x for x in nodes if x.name == bone.name)

		# Extract all our local space transforms
		rotations = []
		translations = []
		scales = []

		for i in range(clip.num_samples):
			time.SetSecondDouble(i * frame_duration)
			matrix = anim_evaluator.GetNodeLocalTransform(bone_node.node, time)

			rotation = matrix.GetQ()
			translation = matrix.GetT()
			scale = matrix.GetS()

			# Convert from FBX types to float arrays
			rotation = quaternion_to_array(rotation)
			translation = vector3_to_array(translation)
			scale = vector3_to_array(scale)

			rotations.append(rotation)
			translations.append(translation)
			scales.append(scale)

		# Clear track if it is constant and default
		if is_track_default(rotations, default_rotation):
			rotations = []

		if is_track_default(translations, default_translation):
			translations = []

		if is_track_default(scales, default_scale):
			scales = []

		track = ACLTrack(bone.name, rotations, translations, scales)
		tracks.append(track)

	return tracks

def print_clip(file, clip):
	print('clip =', file = file)
	print('{', file = file)
	print('\tname = "{}"'.format(clip.name), file = file)
	print('\tnum_samples = {}'.format(clip.num_samples), file = file)
	print('\tsample_rate = {}'.format(clip.sample_rate), file = file)
	print('\terror_threshold = {}'.format(clip.error_threshold), file = file)
	print('}', file = file)
	print('', file = file)

def print_bones(file, bones):
	default_rotation = [ 0.0, 0.0, 0.0, 1.0 ]
	default_translation = [ 0.0, 0.0, 0.0 ]
	default_scale = [ 1.0, 1.0, 1.0 ]

	print('bones =', file = file)
	print('[', file = file)
	for bone in bones:
		print('\t{', file = file)
		print('\t\tname = "{}"'.format(bone.name), file = file)
		print('\t\tparent = "{}"'.format(bone.parent), file = file)
		print('\t\tvertex_distance = {}'.format(bone.vtx_distance), file = file)

		if not is_key_default(bone.bind_rotation, default_rotation):
			print('\t\tbind_rotation = [ {}, {}, {}, {} ]'.format(bone.bind_rotation[0], bone.bind_rotation[1], bone.bind_rotation[2], bone.bind_rotation[3]), file = file)

		if not is_key_default(bone.bind_translation, default_translation):
			print('\t\tbind_translation = [ {}, {}, {} ]'.format(bone.bind_translation[0], bone.bind_translation[1], bone.bind_translation[2]), file = file)

		if not is_key_default(bone.bind_scale, default_scale):
			print('\t\tbind_scale = [ {}, {}, {} ]'.format(bone.bind_scale[0], bone.bind_scale[1], bone.bind_scale[2]), file = file)

		print('\t}', file = file)
	print(']', file = file)
	print('', file = file)

def print_tracks(file, tracks):
	print('tracks =', file = file)
	print('[', file = file)
	for track in tracks:
		if len(track.rotations) + len(track.translations) + len(track.scales) == 0:
			continue

		print('\t{', file = file)
		print('\t\tname = "{}"'.format(track.name), file = file)
		if len(track.rotations) != 0:
			print('\t\trotations =', file = file)
			print('\t\t[', file = file)
			for rotation in track.rotations:
				print('\t\t\t[ {}, {}, {}, {} ]'.format(rotation[0], rotation[1], rotation[2], rotation[3]), file = file)
			print('\t\t]', file = file)

		if len(track.translations) != 0:
			print('\t\ttranslations =', file = file)
			print('\t\t[', file = file)
			for translation in track.translations:
				print('\t\t\t[ {}, {}, {} ]'.format(translation[0], translation[1], translation[2]), file = file)
			print('\t\t]', file = file)

		if len(track.scales) != 0:
			print('\t\tscales =', file = file)
			print('\t\t[', file = file)
			for scale in track.scales:
				print('\t\t\t[ {}, {}, {} ]'.format(scale[0], scale[1], scale[2]), file = file)
			print('\t\t]', file = file)

		print('\t}', file = file)
	print(']', file = file)
	print('', file = file)

def parse_argv():
	options = {}
	options['fbx'] = ""
	options['stack'] = ""
	options['acl'] = ""
	options['zip'] = False

	for i in range(1, len(sys.argv)):
		value = sys.argv[i]

		# TODO: Strip trailing '/' or '\'
		if value.startswith('-fbx='):
			options['fbx'] = value[5:].replace('"', '')

		if value.startswith('-stack='):
			options['stack'] = value[7:].replace('"', '')
		
		if value.startswith('-acl='):
			options['acl'] = value[5:].replace('"', '')

		if value == '-zip':
			options['zip'] = True

	return options

def convert_file(fbx_filename, anim_stack_name, acl_filename, zip):
	# Prepare the FBX SDK.
	sdk_manager, scene = InitializeSdkObjects()

	print('Loading FBX: {}'.format(fbx_filename))
	result = LoadScene(sdk_manager, scene, fbx_filename)

	if not result:
		print('An error occurred while loading the scene!')
		return False
	else:
		print('Parsing FBX...')
		# TODO: Ensure we only have 1 anim layer
		anim_stack = get_animation_stack(scene, anim_stack_name)
		clip = parse_clip(scene, anim_stack)
		nodes = parse_hierarchy(scene)
		bones = parse_bind_pose(scene, nodes)
		tracks = parse_tracks(scene, clip, bones, nodes)

		# If we don't provide an ACL filename, we'll write to STDOUT
		# If we provide an ACL filename but not '-zip', we'll output the raw file
		# If we provide both, we'll only output a zip file with the same name
		if len(acl_filename) != 0:
			if zip:
				zip_filename = acl_filename.replace('.sjson', '.zip')
				print('Writing {}...'.format(zip_filename))
				file = io.StringIO()
			else:
				print('Writing {}...'.format(acl_filename))
				file = open(acl_filename, 'w')
		else:
			file = sys.stdout

		print('version = {}'.format(ACL_FILE_FORMAT_VERSION), file = file)
		print('', file = file)
		print_clip(file, clip)
		print_bones(file, bones)
		print_tracks(file, tracks)

		if len(acl_filename) != 0:
			if zip:
				# LZMA is generally smaller, use ZIP_DEFLATED if it is unsupported on your platform
				# TODO: Detect this automatically somehow?
				#zip_file = zipfile.ZipFile(zip_filename, 'w', zipfile.ZIP_DEFLATED)
				zip_file = zipfile.ZipFile(zip_filename, 'w', zipfile.ZIP_LZMA)
				zip_file.writestr(os.path.basename(acl_filename), file.getvalue())
				zip_file.close()

			file.close()

	# Destroy all objects created by the FBX SDK.
	sdk_manager.Destroy()

	return True

if __name__ == "__main__":
	try:
		from FbxCommon import *
	except ImportError:
		import platform
		msg = 'ERROR: could not import the FBX libraries.  They must be copied into Python\'s site-packages directory, and can only be\nused with Python versions 2.7 and 3.3.  For details, see:\n\n  http://docs.autodesk.com/FBX/2014/ENU/FBX-SDK-Documentation/files/GUID-2F3A42FA-4C19-42F2-BC4F-B9EC64EA16AA.htm'
		print(msg) 
		sys.exit(1)

	options = parse_argv()

	fbx_filename = options['fbx']
	if len(fbx_filename) == 0:
		print('Usage: fbx2acl -fbx=<FBX file name> [-stack=<animation stack name>] [-acl=<ACL file name>] [-zip]')
		sys.exit(1)

	anim_stack_name = options['stack']
	acl_filename = options['acl']
	zip = options['zip']

	if not os.path.exists(fbx_filename):
		print('FBX input not found: {}'.format(fbx_filename))
		sys.exit(1)

	if os.path.isdir(fbx_filename):
		# Our FBX input is a directory, recursively go down and mirror the structure with the output ACL directory
		if not os.path.exists(acl_filename):
			# Create our output directory
			os.makedirs(acl_filename)

		if not os.path.isdir(acl_filename):
			print('An input FBX directory requires an output ACL directory')
			sys.exit(1)

		fbx_dir = fbx_filename
		acl_dir = acl_filename
		for (dirpath, dirnames, filenames) in os.walk(fbx_dir):
			acl_dirname = dirpath.replace(fbx_dir, acl_dir)

			for filename in filenames:
				if not filename.endswith('.fbx'):
					continue

				fbx_filename = os.path.join(dirpath, filename)
				acl_filename = os.path.join(acl_dirname, filename.replace('.fbx', '.acl.sjson'))

				if not os.path.exists(acl_dirname):
					os.makedirs(acl_dirname)

				result = convert_file(fbx_filename, anim_stack_name, acl_filename, zip)
				if not result:
					sys.exit(1)

		sys.exit(0)

	# Convert a single file
	if not acl_filename.endswith('.acl.sjson'):
		print('Invalid ACL filename, it should be of the form *.acl.sjson')
		sys.exit(1)

	result = convert_file(fbx_filename, anim_stack_name, acl_filename, zip)

	if result:
		sys.exit(0)
	else:
		sys.exit(1)
