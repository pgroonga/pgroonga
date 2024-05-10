#define PGRN_DEFINE_LOG_LEVEL_ENTRIES(name)                                    \
	static struct config_enum_entry name[] = {                                 \
		{"none", GRN_LOG_NONE, false},                                         \
		{"emergency", GRN_LOG_EMERG, false},                                   \
		{"alert", GRN_LOG_ALERT, false},                                       \
		{"critical", GRN_LOG_CRIT, false},                                     \
		{"error", GRN_LOG_ERROR, false},                                       \
		{"warning", GRN_LOG_WARNING, false},                                   \
		{"notice", GRN_LOG_NOTICE, false},                                     \
		{"info", GRN_LOG_INFO, false},                                         \
		{"debug", GRN_LOG_DEBUG, false},                                       \
		{"dump", GRN_LOG_DUMP, false},                                         \
		{NULL, GRN_LOG_NONE, false}}
