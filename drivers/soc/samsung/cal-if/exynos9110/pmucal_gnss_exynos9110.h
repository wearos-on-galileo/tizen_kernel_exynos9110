/* individual sequence descriptor for GNSS control - init, reset, release, gnss_active_clear, gnss_reset_req_clear */
struct pmucal_seq gnss_init[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x0040, (0x1 << 21), (0x1 << 21), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x0040, (0x1 << 20), (0x1 << 20), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "EXT_REGULATOR_CON_STATUS", 0x11860000, 0x3644, (0x1 << 0), (0x1 << 0), 0x11860000, 0x3644, (0x1 << 0), (0x1 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x0040, (0x1 << 18), (0x1 << 18), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x0040, (0x1 << 19), (0x1 << 19), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x0040, (0x1 << 1), (0x1 << 1), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_S", 0x11860000, 0x0044, (0x1 << 3), (0x1 << 3), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "RESET_SEQUENCER_STATUS", 0x11860000, 0x0504, (0x7 << 4), (0x5 << 4), 0x11860000, 0x0504, (0x7 << 4), (0x5 << 4)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x0040, (0x1 << 21), (0x0 << 21), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x0040, (0x1 << 18), (0x0 << 18), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "PMU_EVENT_INTERRUPT_ENABLE_GRP3", 0x11860000, 0x7f0c, (0x1 << 13), (0x1 << 13), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "PMU_EVENT_INTERRUPT_ENABLE_GRP3", 0x11860000, 0x7f0c, (0x1 << 29), (0x1 << 29), 0, 0, 0xffffffff, 0),
};
struct pmucal_seq gnss_status[] = {
	PMUCAL_SEQ_DESC(PMUCAL_READ, "RESET_SEQUENCER_STATUS", 0x11860000, 0x0504, (0x7 << 4), 0, 0, 0, 0xffffffff, 0),
};
struct pmucal_seq gnss_reset_assert[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "PMU_EVENT_INTERRUPT_ENABLE_GRP3", 0x11860000, 0x7f0c, (0x1 << 13), (0x0 << 13), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "PMU_EVENT_INTERRUPT_ENABLE_GRP3", 0x11860000, 0x7f0c, (0x1 << 29), (0x0 << 29), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x0040, (0x1 << 20), (0x1 << 20), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x0040, (0x1 << 21), (0x1 << 21), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "EXT_REGULATOR_CON_STATUS", 0x11860000, 0x3644, (0x1 << 0), (0x1 << 0), 0x11860000, 0x3644, (0x1 << 0), (0x1 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x0040, (0x1 << 19), (0x1 << 19), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x0040, (0x1 << 18), (0x1 << 18), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "RESET_AHEAD_GNSS_SYS_PWR_REG", 0x11860000, 0x1340, (0x3 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CLEANY_BUS_GNSS_SYS_PWR_REG", 0x11860000, 0x1344, (0x1 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "LOGIC_RESET_GNSS_SYS_PWR_REG", 0x11860000, 0x1348, (0x3 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "TCXO_GATE_GNSS_SYS_PWR_REG", 0x11860000, 0x134C, (0x1 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_DISABLE_ISO_SYS_PWR_REG", 0x11860000, 0x1350, (0x1 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_RESET_ISO_SYS_PWR_REG", 0x11860000, 0x1354, (0x1 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x0040, (0x1 << 2), (0x1 << 2), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CENTRAL_SEQ_GNSS_CONFIGURATION", 0x11860000, 0x02C0, (0x1 << 16), (0x0 << 16), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "CENTRAL_SEQ_GNSS_STATUS", 0x11860000, 0x02C4, (0xff << 16), (0x80 << 16), 0x11860000, 0x02C4, (0xff << 16), (0x80 << 16)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x0040, (0x1 << 21), (0x0 << 21), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x0040, (0x1 << 18), (0x0 << 18), 0, 0, 0xffffffff, 0),
};
struct pmucal_seq gnss_reset_release[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x0040, (0x1 << 20), (0x1 << 20), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x0040, (0x1 << 21), (0x1 << 21), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "EXT_REGULATOR_CON_STATUS", 0x11860000, 0x3644, (0x1 << 0), (0x1 << 0), 0x11860000, 0x3644, (0x1 << 0), (0x1 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x0040, (0x1 << 19), (0x1 << 19), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x0040, (0x1 << 18), (0x1 << 18), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_S", 0x11860000, 0x0044, (0x1 << 3), (0x1 << 3), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "CENTRAL_SEQ_GNSS_STATUS", 0x11860000, 0x02C4, (0xff << 16), (0x0 << 16), 0x11860000, 0x02C4, (0xff << 16), (0x0 << 16)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x0040, (0x1 << 21), (0x0 << 21), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x0040, (0x1 << 18), (0x0 << 18), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "PMU_EVENT_INTERRUPT_ENABLE_GRP3", 0x11860000, 0x7f0c, (0x1 << 13), (0x1 << 13), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "PMU_EVENT_INTERRUPT_ENABLE_GRP3", 0x11860000, 0x7f0c, (0x1 << 29), (0x1 << 29), 0, 0, 0xffffffff, 0),
};
struct pmucal_seq gnss_gnss_active_clear[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x0040, (0x1 << 6), (0x1 << 6), 0, 0, 0xffffffff, 0),
};
struct pmucal_seq gnss_gnss_reset_req_clear[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS", 0x11860000, 0x0040, (0x1 << 8), (0x1 << 8), 0, 0, 0xffffffff, 0),
};
struct pmucal_gnss pmucal_gnss_list = {
		.init = gnss_init,
		.status = gnss_status,
		.reset_assert = gnss_reset_assert,
		.reset_release = gnss_reset_release,
		.gnss_active_clear = gnss_gnss_active_clear,
		.gnss_reset_req_clear = gnss_gnss_reset_req_clear,
		.num_init = ARRAY_SIZE(gnss_init),
		.num_status = ARRAY_SIZE(gnss_status),
		.num_reset_assert = ARRAY_SIZE(gnss_reset_assert),
		.num_reset_release = ARRAY_SIZE(gnss_reset_release),
		.num_gnss_active_clear = ARRAY_SIZE(gnss_gnss_active_clear),
		.num_gnss_reset_req_clear = ARRAY_SIZE(gnss_gnss_reset_req_clear),
};
unsigned int pmucal_gnss_list_size = 1;
