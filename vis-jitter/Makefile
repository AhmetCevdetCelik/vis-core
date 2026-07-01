# VIS Core Makefile
# Usage:
#   make                   — build vis-jitter
#   make vis-probe         — build portable VIS probe
#   make vis-doctor        — build VIS Doctor
#   make vis-report-*      — build report tooling
#   make test              — run rootless core tests
#   make test-root         — run SMI audit test with sudo
#   make lint              — run lightweight formatting checks
#   make docs-check        — check local Markdown links
#   make clean             — remove generated binaries

CXX      := g++
CXXFLAGS := -Wall -Wextra -O2 -std=c++17 -Iinclude
LDFLAGS  := -lnuma -lpthread

SRC_DIR  := src
INC_DIR  := include
TEST_DIR := tests

CORE_SRCS := $(SRC_DIR)/main.cpp \
             $(SRC_DIR)/measurement.cpp \
             $(SRC_DIR)/vis_platform.cpp \
             $(SRC_DIR)/smi_audit.cpp \
             $(SRC_DIR)/histogram.cpp \
             $(SRC_DIR)/report.cpp

DOCTOR_SRCS := $(SRC_DIR)/doctor_main.cpp \
               $(SRC_DIR)/doctor.cpp \
               $(SRC_DIR)/measurement.cpp \
               $(SRC_DIR)/vis_platform.cpp \
               $(SRC_DIR)/smi_audit.cpp \
               $(SRC_DIR)/histogram.cpp \
               $(SRC_DIR)/report.cpp

PROBE_SRCS := $(SRC_DIR)/vis_probe_main.cpp \
              $(SRC_DIR)/vis_probe.cpp \
              $(SRC_DIR)/vis_probe_semantics.cpp \
              $(SRC_DIR)/vis_platform.cpp

REPORT_VALIDATE_SRCS := $(SRC_DIR)/report_validator_main.cpp \
                        $(SRC_DIR)/report_validator.cpp \
                        $(SRC_DIR)/vis_probe_semantics.cpp

REPORT_REDACT_SRCS := $(SRC_DIR)/report_redactor_main.cpp \
                      $(SRC_DIR)/report_redactor.cpp

REPORT_BUNDLE_SRCS := $(SRC_DIR)/report_bundle_main.cpp \
                      $(SRC_DIR)/report_bundle.cpp \
                      $(SRC_DIR)/report_validator.cpp

REPORT_SUMMARY_SRCS := $(SRC_DIR)/report_summary_main.cpp \
                       $(SRC_DIR)/report_summary.cpp \
                       $(SRC_DIR)/report_validator.cpp \
                       $(SRC_DIR)/vis_probe_semantics.cpp

HDRS := $(INC_DIR)/vis_jitter.hpp \
        $(INC_DIR)/vis_probe.hpp \
        $(INC_DIR)/vis_probe_semantics.hpp \
        $(INC_DIR)/vis_platform.hpp \
        $(INC_DIR)/evidence.hpp \
        $(INC_DIR)/measurement.hpp \
        $(INC_DIR)/smi_audit.hpp \
        $(INC_DIR)/histogram.hpp \
        $(INC_DIR)/report.hpp \
        $(INC_DIR)/report_schema.hpp \
        $(INC_DIR)/report_validator.hpp \
        $(INC_DIR)/report_redactor.hpp \
        $(INC_DIR)/report_bundle.hpp \
        $(INC_DIR)/report_summary.hpp \
        $(INC_DIR)/doctor.hpp

TARGET := vis-jitter
PROBE_BIN := vis-probe
DOCTOR_BIN := vis-doctor
REPORT_VALIDATE_BIN := vis-report-validate
REPORT_REDACT_BIN := vis-report-redact
REPORT_BUNDLE_BIN := vis-report-bundle
REPORT_SUMMARY_BIN := vis-report-summary

TEST_SMI := test_smi_audit
TEST_HIST := test_histogram
TEST_REPORT := test_report_json
TEST_REPORT_VALIDATOR := test_report_validator
TEST_REPORT_REDACTOR := test_report_redactor
TEST_REPORT_BUNDLE := test_report_bundle
TEST_REPORT_SUMMARY := test_report_summary
TEST_API := test_public_api
TEST_PROBE := test_vis_probe
TEST_PLATFORM := test_vis_platform
TEST_DOCTOR := test_doctor

all: $(TARGET)

$(TARGET): $(CORE_SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) -o $@ $(CORE_SRCS) $(LDFLAGS)

$(PROBE_BIN): $(PROBE_SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) -o $@ $(PROBE_SRCS)

$(DOCTOR_BIN): $(DOCTOR_SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) -o $@ $(DOCTOR_SRCS) $(LDFLAGS)

$(REPORT_VALIDATE_BIN): $(REPORT_VALIDATE_SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) -o $@ $(REPORT_VALIDATE_SRCS)

$(REPORT_REDACT_BIN): $(REPORT_REDACT_SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) -o $@ $(REPORT_REDACT_SRCS)

$(REPORT_BUNDLE_BIN): $(REPORT_BUNDLE_SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) -o $@ $(REPORT_BUNDLE_SRCS)

$(REPORT_SUMMARY_BIN): $(REPORT_SUMMARY_SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) -o $@ $(REPORT_SUMMARY_SRCS)

test: $(TEST_HIST) $(TEST_REPORT) $(TEST_REPORT_VALIDATOR) $(TEST_REPORT_REDACTOR) $(TEST_REPORT_BUNDLE) $(TEST_REPORT_SUMMARY) $(TEST_API) $(TEST_PROBE) $(TEST_PLATFORM) $(TEST_DOCTOR)
	@echo "Running test_histogram..."
	./$(TEST_HIST)
	@echo ""
	@echo "Running test_report_json..."
	./$(TEST_REPORT)
	@echo ""
	@echo "Running test_report_validator..."
	./$(TEST_REPORT_VALIDATOR)
	@echo ""
	@echo "Running test_report_redactor..."
	./$(TEST_REPORT_REDACTOR)
	@echo ""
	@echo "Running test_report_bundle..."
	./$(TEST_REPORT_BUNDLE)
	@echo ""
	@echo "Running test_report_summary..."
	./$(TEST_REPORT_SUMMARY)
	@echo ""
	@echo "Running test_public_api..."
	./$(TEST_API)
	@echo ""
	@echo "Running test_vis_probe..."
	./$(TEST_PROBE)
	@echo ""
	@echo "Running test_vis_platform..."
	./$(TEST_PLATFORM)
	@echo ""
	@echo "Running test_doctor..."
	./$(TEST_DOCTOR)

test-root: $(TEST_SMI)
	@echo ""
	@echo "Running test_smi_audit (requires root)..."
	sudo ./$(TEST_SMI)

test-all: test test-root

$(TEST_SMI): $(TEST_DIR)/test_smi_audit.cpp $(SRC_DIR)/smi_audit.cpp $(INC_DIR)/smi_audit.hpp $(INC_DIR)/vis_jitter.hpp
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_DIR)/test_smi_audit.cpp $(SRC_DIR)/smi_audit.cpp $(LDFLAGS)

$(TEST_HIST): $(TEST_DIR)/test_histogram.cpp $(SRC_DIR)/histogram.cpp $(INC_DIR)/histogram.hpp $(INC_DIR)/vis_jitter.hpp
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_DIR)/test_histogram.cpp $(SRC_DIR)/histogram.cpp

$(TEST_REPORT): $(TEST_DIR)/test_report_json.cpp $(SRC_DIR)/report.cpp $(INC_DIR)/report.hpp $(INC_DIR)/vis_jitter.hpp $(INC_DIR)/report_schema.hpp
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_DIR)/test_report_json.cpp $(SRC_DIR)/report.cpp

$(TEST_REPORT_VALIDATOR): $(TEST_DIR)/test_report_validator.cpp $(SRC_DIR)/report_validator.cpp $(INC_DIR)/report_validator.hpp $(INC_DIR)/report_schema.hpp
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_DIR)/test_report_validator.cpp $(SRC_DIR)/report_validator.cpp $(SRC_DIR)/vis_probe_semantics.cpp

$(TEST_REPORT_REDACTOR): $(TEST_DIR)/test_report_redactor.cpp $(SRC_DIR)/report_redactor.cpp $(INC_DIR)/report_redactor.hpp
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_DIR)/test_report_redactor.cpp $(SRC_DIR)/report_redactor.cpp

$(TEST_REPORT_BUNDLE): $(TEST_DIR)/test_report_bundle.cpp $(SRC_DIR)/report_bundle.cpp $(SRC_DIR)/report_validator.cpp $(INC_DIR)/report_bundle.hpp $(INC_DIR)/report_validator.hpp $(INC_DIR)/report_schema.hpp
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_DIR)/test_report_bundle.cpp $(SRC_DIR)/report_bundle.cpp $(SRC_DIR)/report_validator.cpp $(SRC_DIR)/vis_probe_semantics.cpp

$(TEST_REPORT_SUMMARY): $(TEST_DIR)/test_report_summary.cpp $(SRC_DIR)/report_summary.cpp $(SRC_DIR)/report_validator.cpp $(INC_DIR)/report_summary.hpp $(INC_DIR)/report_validator.hpp $(INC_DIR)/report_schema.hpp
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_DIR)/test_report_summary.cpp $(SRC_DIR)/report_summary.cpp $(SRC_DIR)/report_validator.cpp $(SRC_DIR)/vis_probe_semantics.cpp

$(TEST_API): $(TEST_DIR)/test_public_api.cpp $(SRC_DIR)/measurement.cpp $(SRC_DIR)/smi_audit.cpp $(SRC_DIR)/histogram.cpp $(INC_DIR)/measurement.hpp $(INC_DIR)/smi_audit.hpp $(INC_DIR)/histogram.hpp $(INC_DIR)/vis_jitter.hpp $(INC_DIR)/report_schema.hpp
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_DIR)/test_public_api.cpp $(SRC_DIR)/measurement.cpp $(SRC_DIR)/vis_platform.cpp $(SRC_DIR)/smi_audit.cpp $(SRC_DIR)/histogram.cpp $(LDFLAGS)

$(TEST_PROBE): $(TEST_DIR)/test_vis_probe.cpp $(SRC_DIR)/vis_probe.cpp $(SRC_DIR)/vis_platform.cpp $(INC_DIR)/vis_probe.hpp $(INC_DIR)/vis_platform.hpp
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_DIR)/test_vis_probe.cpp $(SRC_DIR)/vis_probe.cpp $(SRC_DIR)/vis_probe_semantics.cpp $(SRC_DIR)/vis_platform.cpp

$(TEST_PLATFORM): $(TEST_DIR)/test_vis_platform.cpp $(SRC_DIR)/vis_platform.cpp $(INC_DIR)/vis_platform.hpp
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_DIR)/test_vis_platform.cpp $(SRC_DIR)/vis_platform.cpp

$(TEST_DOCTOR): $(TEST_DIR)/test_doctor.cpp $(SRC_DIR)/doctor.cpp $(SRC_DIR)/measurement.cpp $(SRC_DIR)/smi_audit.cpp $(SRC_DIR)/histogram.cpp $(SRC_DIR)/report.cpp $(INC_DIR)/doctor.hpp $(INC_DIR)/vis_jitter.hpp $(INC_DIR)/report_schema.hpp
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_DIR)/test_doctor.cpp $(SRC_DIR)/doctor.cpp $(SRC_DIR)/measurement.cpp $(SRC_DIR)/vis_platform.cpp $(SRC_DIR)/smi_audit.cpp $(SRC_DIR)/histogram.cpp $(SRC_DIR)/report.cpp $(LDFLAGS)

format-check:
	../scripts/check-format.sh

cppcheck:
	../scripts/run-cppcheck.sh

lint: format-check

lint-extra: lint cppcheck

docs-check:
	../scripts/check-doc-links.sh

clean:
	rm -f $(TARGET) $(PROBE_BIN) $(DOCTOR_BIN) $(REPORT_VALIDATE_BIN) $(REPORT_REDACT_BIN) $(REPORT_BUNDLE_BIN) $(REPORT_SUMMARY_BIN) $(TEST_SMI) $(TEST_HIST) $(TEST_REPORT) $(TEST_REPORT_VALIDATOR) $(TEST_REPORT_REDACTOR) $(TEST_REPORT_BUNDLE) $(TEST_REPORT_SUMMARY) $(TEST_API) $(TEST_PROBE) $(TEST_PLATFORM) $(TEST_DOCTOR)

.PHONY: all test test-root test-all format-check cppcheck lint lint-extra docs-check clean
