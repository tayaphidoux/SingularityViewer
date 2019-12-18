# -*- cmake -*-
include(Prebuilt)
include(Variables)

if (USE_NVAPI)
  if (WINDOWS)
    use_prebuilt_binary(nvapi)
    if (WORD_SIZE EQUAL 32)
      set(NVAPI_LIBRARY nvapi)
    elseif (WORD_SIZE EQUAL 64)
      set(NVAPI_LIBRARY nvapi64)
    endif (WORD_SIZE EQUAL 32)	
  else (WINDOWS)
    set(NVAPI_LIBRARY "")
  endif (WINDOWS)
else (USE_NVAPI)
  set(NVAPI_LIBRARY "")
endif (USE_NVAPI)

