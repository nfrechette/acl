# The ACL file format

To keep the library as simple and minimal as possible, it does not support common animation file formats as is. There are too many out there and many of them would require linking 3rd party libraries, adding dependencies in the process. It is assumed that most games already have a pipeline to load animation files and perhaps even pre-process them in some way before handing the data over to the compression algorithm. As such, for our internal testing purposes and for debugging, a [Simplified JSON](http://help.autodesk.com/view/Stingray/ENU/?guid=__stingray_help_managing_content_sjson_html) file is used to represent an animation clip. These files can easily be compressed with gzip and be read by a human.

Small auxiliary tools exist to convert to this format such as the [fbx2acl](https://github.com/nfrechette/acl/blob/develop/tools/fbx2acl/fbx2acl.py) python script. The library also supports writing ACL files from a populated [skeleton and clip](https://github.com/nfrechette/acl/blob/develop/includes/acl/io/clip_writer.h).

It is important to note that this file format is not meant to be used in production (although you could). It's primary purpose is to help us debug and reproduce issues by attaching an ACL file to bug reports.

A reference ACL file can be found [here](https://github.com/nfrechette/acl/blob/develop/tools/format_reference.acl.js).
