# Convenience wrappers around the ESPHome CLI.
#   make config   - validate the YAML (seconds, no toolchain needed)
#   make compile  - build the firmware
#   make run      - build and flash (USB first time, OTA afterwards)
#   make logs     - tail the device log
#   make clean    - remove the build directory (fixes stale-include problems)

YAML    ?= gear-lever.yaml
ESPHOME ?= esphome

.PHONY: config compile run logs clean secrets

secrets:
	@test -f secrets.yaml || { cp secrets.yaml.example secrets.yaml; echo "Created secrets.yaml from example: edit it with your WiFi credentials."; }

config: secrets
	$(ESPHOME) config $(YAML)

compile: secrets
	$(ESPHOME) compile $(YAML)

run: secrets
	$(ESPHOME) run $(YAML)

logs:
	$(ESPHOME) logs $(YAML)

clean:
	$(ESPHOME) clean $(YAML)
	rm -rf .esphome
