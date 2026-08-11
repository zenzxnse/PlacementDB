function(placedb_enable_warnings target_name)
  # Warnings shared by GCC and Clang. -Wold-style-cast and -Wuseless-cast are
  # deliberately absent: libsodium and Drogon macros expand C-style casts
  # inside project translation units.
  set(warnings
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
    -Wsign-conversion
    -Wnon-virtual-dtor
    -Woverloaded-virtual
    -Wnull-dereference
    -Wdouble-promotion
    -Wimplicit-fallthrough
    -Wformat=2
    -Wcast-align
    -Wmisleading-indentation
  )

  # GCC-only analyses without a Clang equivalent spelling.
  set(gcc_warnings
    -Wduplicated-cond
    -Wduplicated-branches
    -Wlogical-op
  )

  target_compile_options(
    "${target_name}"
    INTERFACE
      "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:${warnings}>"
      "$<$<COMPILE_LANG_AND_ID:CXX,GNU>:${gcc_warnings}>"
      "$<$<AND:$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>,$<BOOL:${PLACEDB_WARNINGS_AS_ERRORS}>>:-Werror>"
  )

  # Debug configurations also arm the libstdc++ container and iterator
  # assertions. They are cheap and catch out-of-range and invalidated-iterator
  # use that plain -Wall never sees.
  target_compile_definitions(
    "${target_name}"
    INTERFACE
      "$<$<AND:$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>,$<CONFIG:Debug>>:_GLIBCXX_ASSERTIONS>"
  )
endfunction()
