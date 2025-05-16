#pragma once

#define ERAY_DELETE_COPY_AND_MOVE(ClassName)       \
  ClassName(const ClassName&)            = delete; \
  ClassName(ClassName&&)                 = delete; \
  ClassName& operator=(const ClassName&) = delete; \
  ClassName& operator=(ClassName&&)      = delete;

#define ERAY_DELETE_COPY(ClassName)                \
  ClassName(const ClassName&)            = delete; \
  ClassName& operator=(const ClassName&) = delete;

#define ERAY_DELETE_MOVE(ClassName)           \
  ClassName(ClassName&&)            = delete; \
  ClassName& operator=(ClassName&&) = delete;

#define ERAY_DEFAULT_MOVE(ClassName)           \
  ClassName(ClassName&&)            = default; \
  ClassName& operator=(ClassName&&) = default;

#define ERAY_DELETE_MOVE_AND_COPY_ASSIGN(ClassName) \
  ClassName& operator=(const ClassName&) = delete;  \
  ClassName& operator=(ClassName&&)      = delete;

#define ERAY_DEFAULT_MOVE_AND_COPY_ASSIGN(ClassName) \
  ClassName& operator=(const ClassName&) = delete;   \
  ClassName& operator=(ClassName&&)      = delete;

#define ERAY_DEFAULT_MOVE_AND_COPY_CTOR(ClassName) \
  ClassName(ClassName&&) noexcept = default;       \
  ClassName(const ClassName&)     = default;
