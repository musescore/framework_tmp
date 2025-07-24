# framework_tmp

> [!WARNING]  
> This is a temporary repository for the Muse Framework. 
> Everything is about to change.

Muse Framework is a collection of libraries and utilities that are used by both MuseScore and Audacity.
It is currently used inside [MuseScore 4](https://github.com/musescore/musescore) and [Audacity 4](https://github.com/audacity/audacity).

## General Idea
The content of **musescore** repo:
- `musescore/src/framework`

is kept in sync with **audacity** repo:
- `audacity/_deps/muse_framework-src/framework`

where `muse_framework-src` is the name of the subdirectory where the content is fetched from this repository `framework_tmp`.

## How changes from musescore are propagated to audacity?

1. Changes are made in `musescore/src/framework`
2. You fork `framework_tmp` and create a branch named `update_250724` (Today's date in format YYYYMMDD)
3. `musescore/src/framework` is copied to `framework_tmp/framework` (make sure to remove any content from `framework_tmp/framework` before pasting the content from `musescore/src/framework`)
4. You commit and push the changes to your fork 
5. You create a PR from your fork `update_250724` to `main` in `framework_tmp`
6. PR is merged to `main` in `framework_tmp`
7. A Tag to this new commit is created in `framework_tmp` (please kindly ask maintainers)
8. In Audacity repo `audacity/_deps/muse_framework-src` is updated to point to the latest commit in `framework_tmp` this is done inside the `audacity/CMakeLists.txt` file
```cmake
FetchContent_Declare(
  muse_framework
  GIT_REPOSITORY https://github.com/musescore/framework_tmp.git
  GIT_TAG        u250618_1
)
```
_most probably the tag name will be `u250724` (you will have to replace the old tag here `u250618_1` by `u250724`)_

9. The updated `audacity/CMakeLists.txt` is committed and pushed to `audacity` repository inside your PR.

That's it, you should have a PR in `audacity` repository that updates the framework to the latest version.

## How changes from audacity are propagated to musescore?

Currently **musescore** doesn't use a separated repo for the framework, so the changes are propagated manually by musescore maintainers inside the musescore repository.

To help them you still can create a PR in `framework_tmp` repository that updates the framework to the latest version.

Thanks.
