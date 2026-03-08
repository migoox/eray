include(../../../cmake/CPM.cmake)

CPMAddPackage(
  NAME googletest
  GITHUB_REPOSITORY btzy/nativefiledialog-extended
  GIT_TAG 1.2.1v
  SYSTEM YES

  OPTIONS 
  # IMPORTANT: The xdg-desktop-portal allows to display the native file dialog
  # on linux systems instead of the gtk file dialog that is used by default. 
  # However not every DEs supports them, see https://wiki.archlinux.org/title/XDG_Desktop_Portal.
  "NFD_PORTAL ON"
)