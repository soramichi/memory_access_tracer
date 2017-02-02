// some variables that are referenced by but not included in libperf.a
// never care the values, as they are not used!
unsigned int scripting_max_stack = 0;
int use_browser = -1;
const char perf_version_string[] = "";

// proto-types. never used. just to surpress warnings.
int script_spec_register(void);
int kvm_exit_event(void);
int kvm_entry_event(void);
int exit_event_begin(void);
int exit_event_end(void);
int exit_event_decode_key(void);
int test__dwarf_unwind(void);

/** defined in builtin-script.o, which should not be linked to libperf.a **/
int script_spec_register(void){
  return 0;
}
/**************************************************************************/


/** defined in buitin-kvm.o, which should not be linked to libperf.a **/
int kvm_exit_event(void){
  return 0;
}

int kvm_entry_event(void){
  return 0;
}

int exit_event_begin(void){
  return 0;
}

int exit_event_end(void){
  return 0;
}

int exit_event_decode_key(void){
  return 0;
}
/*********************************************************************/

/** defined in tests/dwarf-unwind.o, which should not be linked to libperf.a **/
int test__dwarf_unwind(void){
  return 0;
}
/******************************************************************************/

