# fbx2acl

This python script uses the FBK SDK in order to convert animation clips within an FBX file into the [ACL file format](../../docs/the_acl_file_format.md) suitable for the [acl_compressor](../acl_compressor) tool.

Usage: `python fbx2acl -fbx=<FBX file name> [-stack=<animation stack name>] [-start=<time>] [-end=<time>] [-acl=<ACL file name>] [-zip]`

Optional arguments are enclosed in brackets. E.g: `[-end=<time>]`

Note that the script supports converting a whole directory tree if you pass a directory instead of a filename. The output ACL files will mirror the input tree structure.
