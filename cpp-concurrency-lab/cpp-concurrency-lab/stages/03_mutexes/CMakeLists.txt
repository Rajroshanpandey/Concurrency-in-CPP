lab_stage(03)
lab_demo(s03_easy_counter_lock_guard RACY TEST_ARGS 200000)
lab_demo(s03_medium_thread_safe_logger)
# RACY here also covers "intentional deadlock-prone design": TSan reports lock-order-inversion - which is the lesson.
lab_demo(s03_tough_deadlock_lab RACY)
lab_demo(s03_tough_dining_philosophers RACY TEST_ARGS 300)
