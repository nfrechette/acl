[![CLA assistant](https://cla-assistant.io/readme/badge/nfrechette/acl)](https://cla-assistant.io/nfrechette/acl)
[![All Contributors](https://img.shields.io/github/all-contributors/nfrechette/acl)](#contributors-)
[![Build status](https://ci.appveyor.com/api/projects/status/8h1jwmhumqh9ie3h/branch/develop?svg=true)](https://ci.appveyor.com/project/nfrechette/acl)
[![Build status](https://github.com/nfrechette/acl/workflows/build/badge.svg)](https://github.com/nfrechette/acl/actions)
[![Sonar Status](https://sonarcloud.io/api/project_badges/measure?project=nfrechette_acl&metric=alert_status)](https://sonarcloud.io/dashboard?id=nfrechette_acl)
[![GitHub release](https://img.shields.io/github/release/nfrechette/acl.svg)](https://github.com/nfrechette/acl/releases)
[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/nfrechette/acl/master/LICENSE)
[![Discord](https://img.shields.io/discord/691048241864769647?label=discord)](https://discord.gg/UERt4bS)

# Animation Compression Library

Animation compression is a fundamental aspect of modern video game engines. Not only is it important to keep the memory footprint down but it is also critical to keep the animation clip sampling performance fast.

The more memory an animation clip consumes, the slower it will be to sample it and extract a character pose at runtime. For these reasons, any game that attempts to push the boundaries of what the hardware can achieve will at some point need to implement some form of animation compression.

While some degree of compression can easily be achieved with simple tricks, achieving high compression ratios, fast decompression, while simultaneously not compromising the accuracy of the resulting compressed animation requires a great deal of care.

## Goals

This library has four primary goals:

*  Implement state of the art and production ready animation compression algorithms
*  Be easy to integrate into modern video game engines
*  Serve as a benchmark to compare various techniques against one another
*  Document what works and doesn't work

Algorithms are optimized with a focus on (in this particular order):

*  Minimizing the compression artifacts in order to reach high cinematographic quality
*  Fast decompression on all our supported hardware
*  A small memory footprint to lower memory pressure at runtime as well as reducing disk and network usage

Decompression speed will not be sacrificed for a smaller memory footprint nor will accuracy be compromised under any circumstances.

## Philosophy

Much thought was put into designing the library for it to be as flexible and powerful as possible. To this end, the following decisions were made:

*  The library consists of **100% C++11** header files and is thus easy to integrate in any game engine
*  [An intermediary clip format](./docs/the_acl_file_format.md) is supported in order to facilitate debugging and bug reporting
*  All allocations use a [game provided allocator](./includes/acl/core/iallocator.h)
*  All asserts use a [game provided macro](./includes/acl/core/error.h)

## Supported platforms

*  Windows VS2015 x86 and x64
*  Windows (VS2017, VS2019) x86, x64, and ARM64
*  Windows VS2019 with clang9 x86 and x64
*  Linux (gcc 5 to 10) x86 and x64
*  Linux (clang 4 to 11) x86 and x64
*  OS X (Xcode 10.3) x86 and x64
*  OS X (Xcode 11.2) x64
*  Android (NDK 21) ARMv7-A and ARM64
*  iOS (Xcode 10.3, 11.2) ARM64
*  Emscripten (1.39.11) WASM

The above supported platform list is only what is tested every release but if it compiles, it should run just fine.

Note: *VS2017* and *VS2019* compile with *ARM64* on *AppVeyor* but I have no device to test them with.

The [Unreal Engine](https://www.unrealengine.com/en-US/blog) is supported through a plugin found [here](https://github.com/nfrechette/acl-ue4-plugin).

## Getting started

This library is **100%** headers as such you just need to include them in your own project to start using it. However, if you wish to run the unit tests, regression tests, to contribute to ACL or use it for research, head on over to the [getting started](./docs/getting_started.md) section in order to setup your environment and make sure to check out the [contributing guidelines](CONTRIBUTING.md).

If you would like to integrate ACL into your own game engine, follow the integration instructions [here](./docs#how-to-integrate-the-library).

## Performance metrics

*  [Carnegie-Mellon University database performance](./docs/cmu_performance.md)
*  [Paragon database performance](./docs/paragon_performance.md)
*  [Matinee fight scene performance](./docs/fight_scene_performance.md)
*  [Decompression performance](./docs/decompression_performance.md)

## External dependencies

You don't need anything else to get started: everything is self contained.
See [here](./external) for details.

## License, copyright, and code of conduct

This project uses the [MIT license](LICENSE).

Copyright (c) 2017 Nicholas Frechette & Animation Compression Library contributors

Please note that this project is released with a [Contributor Code of Conduct](CODE_OF_CONDUCT.md). By participating in this project you agree to abide by its terms.


## Contributors ✨

Thanks goes to these wonderful people ([emoji key](https://allcontributors.org/docs/en/emoji-key)):

<!-- ALL-CONTRIBUTORS-LIST:START - Do not remove or modify this section -->
<!-- prettier-ignore-start -->
<!-- markdownlint-disable -->
<table>
  <tr>
    <td align="center"><a href="https://github.com/CodyDWJones"><img src="https://avatars.githubusercontent.com/u/28773740?v=4?s=100" width="100px;" alt=""/><br /><sub><b>CodyDWJones</b></sub></a><br /><a href="https://github.com/nfrechette/acl/commits?author=CodyDWJones" title="Code">💻</a> <a href="#data-CodyDWJones" title="Data">🔣</a> <a href="#maintenance-CodyDWJones" title="Maintenance">🚧</a> <a href="#tool-CodyDWJones" title="Tools">🔧</a> <a href="#infra-CodyDWJones" title="Infrastructure (Hosting, Build-Tools, etc)">🚇</a> <a href="#ideas-CodyDWJones" title="Ideas, Planning, & Feedback">🤔</a></td>
    <td align="center"><a href="https://github.com/Meradrin"><img src="https://avatars.githubusercontent.com/u/7066278?v=4?s=100" width="100px;" alt=""/><br /><sub><b>Meradrin</b></sub></a><br /><a href="https://github.com/nfrechette/acl/commits?author=Meradrin" title="Code">💻</a></td>
    <td align="center"><a href="https://github.com/tirpidz"><img src="https://avatars.githubusercontent.com/u/9991876?v=4?s=100" width="100px;" alt=""/><br /><sub><b>Martin Turcotte</b></sub></a><br /><a href="https://github.com/nfrechette/acl/commits?author=tirpidz" title="Code">💻</a> <a href="#tool-tirpidz" title="Tools">🔧</a> <a href="#ideas-tirpidz" title="Ideas, Planning, & Feedback">🤔</a></td>
    <td align="center"><a href="https://github.com/vjeffh"><img src="https://avatars.githubusercontent.com/u/22382688?v=4?s=100" width="100px;" alt=""/><br /><sub><b>vjeffh</b></sub></a><br /><a href="https://github.com/nfrechette/acl/commits?author=vjeffh" title="Code">💻</a></td>
    <td align="center"><a href="https://github.com/Romain-Piquois"><img src="https://avatars.githubusercontent.com/u/3689912?v=4?s=100" width="100px;" alt=""/><br /><sub><b>Romain-Piquois</b></sub></a><br /><a href="https://github.com/nfrechette/acl/issues?q=author%3ARomain-Piquois" title="Bug reports">🐛</a></td>
    <td align="center"><a href="https://github.com/janisozaur"><img src="https://avatars.githubusercontent.com/u/550290?v=4?s=100" width="100px;" alt=""/><br /><sub><b>Michał Janiszewski</b></sub></a><br /><a href="https://github.com/nfrechette/acl/commits?author=janisozaur" title="Code">💻</a> <a href="#tool-janisozaur" title="Tools">🔧</a> <a href="#maintenance-janisozaur" title="Maintenance">🚧</a> <a href="#infra-janisozaur" title="Infrastructure (Hosting, Build-Tools, etc)">🚇</a></td>
    <td align="center"><a href="http://keybase.io/visualphoenix"><img src="https://avatars.githubusercontent.com/u/394175?v=4?s=100" width="100px;" alt=""/><br /><sub><b>Raymond Barbiero</b></sub></a><br /><a href="#ideas-visualphoenix" title="Ideas, Planning, & Feedback">🤔</a></td>
  </tr>
</table>

<!-- markdownlint-restore -->
<!-- prettier-ignore-end -->

<!-- ALL-CONTRIBUTORS-LIST:END -->

This project follows the [all-contributors](https://github.com/all-contributors/all-contributors) specification. Contributions of any kind welcome!
