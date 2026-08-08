function(placedb_enable_sanitizers target_name)
  if(PLACEDB_ENABLE_ASAN)
    target_compile_options(
      "${target_name}"
      INTERFACE
        "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-fsanitize=address;-fno-omit-frame-pointer>"
    )
    target_link_options(
      "${target_name}"
      INTERFACE
        "$<$<LINK_LANG_AND_ID:CXX,GNU,Clang>:-fsanitize=address>"
    )
  endif()

  if(PLACEDB_ENABLE_UBSAN)
    target_compile_options(
      "${target_name}"
      INTERFACE
        "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-fsanitize=undefined;-fno-omit-frame-pointer>"
    )
    target_link_options(
      "${target_name}"
      INTERFACE
        "$<$<LINK_LANG_AND_ID:CXX,GNU,Clang>:-fsanitize=undefined>"
    )
  endif()
endfunction()
