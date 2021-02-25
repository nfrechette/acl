# Database and streaming support

ACL 2.0 now supports compressing multiple animation clips into a single database to facilitate streaming as well as data stripping. See [here](https://nfrechette.github.io/2021/01/17/progressive_animation_streaming/) for details on what it can do and how it works.

See [here](https://github.com/nfrechette/acl-ue4-plugin) to take a look at how the UE4 plugin supports it.

## How to build a database

In order to be able to build a database with the right key frames, metadata must be added during compression. To do so, make sure to enable `include_contributing_error` inside your `acl::compression_settings`.

```c++
acl::compression_settings settings = acl::get_default_compression_settings();
settings.include_contributing_error = true;
```

This will add metadata to the compressed byte stream which will later be stripped once the database is created. Its footprint is only temporary as it isn't needed at runtime when decompressing.

The database creation API is exposed in [acl/compression/compress.h](../includes/acl/compression/compress.h).

First, use `build_database(..)` to create a database. It takes as input the compressed animation clips you wish to merge and it will output new compressed animation clips and the database they are bound to. All of these buffers are binary blobs and can be moved around with `std::memcpy` safely. The only requirement is that they be 16 bytes aligned.

Once the database is created, its bulk data (the part that can be optionally streamed) will be part of the database byte buffer. To strip it into a separate buffer that can be omitted or streamed later, use `split_database_bulk_data(..)`. This will output a new database along with its two bulk data buffers.

If some quality tiers aren't necessary on your platform of choice (e.g. mobile), you can strip them by calling `strip_database_quality_tier(..)`. The bulk data does not change and if it had been stripped, the stripped tier's buffer can simply be freed.

## Decompressing with a database

At runtime, animation clips that are bound to a database can be decompressed without the database. If you attempt to do so, only the data within the clip will be used (lowest visual quality).

In order to use a database, a [acl::database_context](../includes/decompression/database/database.h) must be created for it.

```c++
const acl::compressed_database* database = ...
acl::iallocator& allocator = ...

acl::database_context<acl::default_database_settings> database_context;
database_context.initialize(allocator, *database);
```

The above will work if the database contains its bulk data inline (if it hasn't been stripped). To support streaming (whether from memory, memory mapped, network, or other IO) a [acl::database_streamer](../includes/acl/decompression/database/database_streamer.h) must be provided. See the linked header for details on how to implement it.

```c++
acl::null_database_streamer medium_streamer(medium_data, medium_data_size);
acl::null_database_streamer low_streamer(low_data, low_data_size);

acl::database_context<acl::default_database_settings> database_context;
database_context.initialize(allocator, *database, medium_streamer, low_streamer);
```

If a quality tier has been stripped, its streamer will never be used and any streamer can be provided. Streamers must live as long as the database does. The streamers are responsible for streaming data in and out.

When the time comes to decompress, simply provide the database context alongside the compressed tracks data and make sure database support is enabled in your decompression settings (by default that code is stripped).

```c++
struct my_decompression_settings : acl::default_transform_decompression_settings
{
	using database_settings_type = default_database_settings;
};

const acl::compressed_tracks* compressed_clip_data = ...

acl::decompression_context<my_decompression_settings> context;
context.initialize(compressed_clip_data, database_context);
```

The rest of the decompression code remains unchanged.

It is safe to stream in data while decompression is in progress. Doing so it thread safe. However, only a single stream in/out request can be in flight at a time and streaming out cannot be done while decompression is in progress.

Once a streamer finishes a read request (e.g. file IO), it can complete the stream request from any thread.
