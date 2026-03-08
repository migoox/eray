include(../../../cmake/CPM.cmake)

CPMAddPackage(
  NAME tracy 
  GITHUB_REPOSITORY wolfpld/tracy
  GIT_TAG v0.13.1 
  SYSTEM YES
)