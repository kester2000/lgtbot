if (WITH_TEST)
  add_executable(test_factory_funner_engine ${CMAKE_CURRENT_LIST_DIR}/engine_test.cc)
  add_test(NAME test_factory_funner_engine COMMAND test_factory_funner_engine)
endif()
