# detex-compress
Fast texture compression utility that makes use of the detex library.

As opposed to texgenpack, which uses a general and versatile but slow compression method, detex-compress uses relatively fast compression algorithms specifically optimized for a texture compression format. In the current version, only BC1 (S3TC), BC2, BC3 and RGTC1 (BC4) compression are supported. The utility is multi-threaded and can compress a 1024x1024 BC1 texture in about 15 seconds with good quality.

The program makes uses of detex (https://github.com/hglm/detex) and DataSetTurbo (https://github.com/hglm/DataSetTurbo).

It is the intention that support for more compression formats will added in the future.
