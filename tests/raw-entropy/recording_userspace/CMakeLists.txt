function(testprogram name)
    add_executable(${name} ${name}.c)
    target_link_libraries(${name} ${PROJECT_NAME})
    target_compile_options(${name}
      PRIVATE
        $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-reserved-macro-identifier>
        $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-language-extension-token>
        $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-padded>
    )
endfunction()

testprogram(jitterentropy-rng)

