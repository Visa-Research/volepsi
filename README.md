# Vole-PSI


Vole-PSI implements the protocols described in [VOLE-PSI: Fast OPRF and Circuit-PSI from Vector-OLE](https://eprint.iacr.org/2021/266) and [Blazing Fast PSI from Improved OKVS and Subfield VOLE](misc/blazingFastPSI.pdf). The library implements stardnard [Private Set Intersection (PSI)](https://en.wikipedia.org/wiki/Private_set_intersection) along with a variant called Circuit PSI where the result is secret shared between the two parties.

The library is cross platform (win,linux,mac) and depends on [libOTe](https://github.com/osu-crypto/libOTe), [sparsehash](https://github.com/sparsehash/sparsehash), [Coproto].

### Build

The library can be cloned and built as
```
git clone ...
cd volePSI
python3 build.py
```
Alternatively the user can manualy call cmake.
The output library `volePSI` and executable `frontend` will be written to `out/build/<platform>/`.
By defaut dependancies are automaticly downloaded.

### Installing

The library and any fetched dependancies can be installed. 
```
python3 build.py --install
```
or 
```
python3 build.py --install=install/prefix/path
```
if a custom install prefix is perfered. Install can also be performed via cmake.

### Dependancy Managment

By default the depenancies are fetched automaticly. This can be turned off by using cmake directly or adding `-D FETCH_AUTO=OFF`. For other options see the cmake output or that of `python build.py --help`.

If the dependancie is installed to the system, then cmake should automaticly find it. If they are installed to a specific location, then you call tell cmake about them as 
```
python3 build.py -D CMAKE_PREFIX_PATH=install/prefix/path
```

