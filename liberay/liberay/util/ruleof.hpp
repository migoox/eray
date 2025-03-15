#pragma once

#define ERAY_DISABLE_COPY_AND_MOVE(ClassName)      \
  ClassName(const ClassName&)            = delete; \
  ClassName(ClassName&&)                 = delete; \
  ClassName& operator=(const ClassName&) = delete; \
  ClassName& operator=(ClassName&&)      = delete;

#define ERAY_DISABLE_COPY(ClassName)               \
  ClassName(const ClassName&)            = delete; \
  ClassName& operator=(const ClassName&) = delete;

#define ERAY_DISABLE_MOVE(ClassName)          \
  ClassName(ClassName&&)            = delete; \
  ClassName& operator=(ClassName&&) = delete;

#define ERAY_DEFAULT_MOVE(ClassName)           \
  ClassName(ClassName&&)            = default; \
  ClassName& operator=(ClassName&&) = default;
