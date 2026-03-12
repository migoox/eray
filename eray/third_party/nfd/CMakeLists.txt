include(CPM)

CPMAddPackage(
  NAME nfd 
  GITHUB_REPOSITORY btzy/nativefiledialog-extended
  GIT_TAG v1.3.0
  SYSTEM YES

  OPTIONS 
  # IMPORTANT: The xdg-desktop-portal allows to display the native file dialog
  # on linux systems instead of the gtk file dialog that is used by default. 
  # However not every DEs supports them, see https://wiki.archlinux.org/title/XDG_Desktop_Portal.
  "NFD_PORTAL ON"
)

add_library(third_party::nfd ALIAS nfd)