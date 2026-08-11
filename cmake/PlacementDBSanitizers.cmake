function(placedb_enable_sanitizers target_name)
  if(PLACEDB_ENABLE_ASAN)
    # ASan on Linux includes LeakSanitizer at process exit by default.
    target_compile_options(
      "${target_name}"
      INTERFACE
        "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-fsanitize=address;-fno-omit-frame-pointer;-fno-optimize-sibling-calls>"
    )
    target_link_options(
      "${target_name}"
      INTERFACE
        "$<$<LINK_LANG_AND_ID:CXX,GNU,Clang>:-fsanitize=address>"
    )
  endif()

  if(PLACEDB_ENABLE_UBSAN)
    # float-divide-by-zero is not part of -fsanitize=undefined. Recovery is
    # disabled so any detected UB aborts the test instead of logging and
    # continuing to a green exit status.
    target_compile_options(
      "${target_name}"
      INTERFACE
        "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-fsanitize=undefined,float-divide-by-zero;-fno-sanitize-recover=all;-fno-omit-frame-pointer>"
    )
    target_link_options(
      "${target_name}"
      INTERFACE
        "$<$<LINK_LANG_AND_ID:CXX,GNU,Clang>:-fsanitize=undefined,float-divide-by-zero>"
    )
  endif()

  if(PLACEDB_ENABLE_TSAN)
    target_compile_options(
      "${target_name}"
      INTERFACE
        "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-fsanitize=thread;-fno-omit-frame-pointer>"
    )
    target_link_options(
      "${target_name}"
      INTERFACE
        "$<$<LINK_LANG_AND_ID:CXX,GNU,Clang>:-fsanitize=thread>"
    )
  endif()
endfunction()
