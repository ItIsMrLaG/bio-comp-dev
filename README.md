# bio_comp_dev

## Implemented
> `uname -r`: `6.10.12-200.fc40.x86_64` 
### Empty-based block device
> just proxy IO-requests
### LZ4-based block device (only linear )
* storing heteromorphic blocks _(both compressed and uncompressed at the same time)_
* mapping: 
    * linear (`lba == pba`)
* compression mods:
    * `LZ4_compress_default` -- comp_prf_id: `0`
    * `LZ4_compress_fast` -- comp_prf_id: `[0..15]` <=> acceleration factor
    * `LZ4_compress_HC` -- comp_prf_id: `[16..31]` <=> `[1..16]` HC-compressionLevel
* decompression 
    * `LZ4_decompress_fast` -- comp_prf_id: `0`
    * `LZ4_decompress_safe` -- comp_prf_id: `1`
* supported block-size(**bs**): `4k`, `8k`, `16k`, `32k`, `64k`, `128k`
    * **bs** selected during device configuration
    * IO-requests that are not multiples of the selected **bs** are not supported

## Plans
1. Non-linear mapping
2. Support for IO-requests that are not multiples of the selected bs
3. ZSTD

## Requirements
* [**fio**](https://fio.readthedocs.io/en/latest/fio_doc.html) for tests
* **lz4** module (`modprobe lz4`)
* **lz4hc** module (`modprobe lz4hc`)
