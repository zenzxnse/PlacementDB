function(placedb_enable_warnings target_name)
  set(warnings
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
    -Wsign-conversion
  )

  target_compile_options(
    "${target_name}"
    INTERFACE
      "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:${warnings}>"
      "$<$<AND:$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>,$<BOOL:${PLACEDB_WARNINGS_AS_ERRORS}>>:-Werror>"
  )
endfunction()
