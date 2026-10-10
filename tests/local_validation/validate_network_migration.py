#!/usr/bin/env python3
"""Run target shell network helpers against fake commands and temporary paths."""
import os
from pathlib import Path
import stat
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HELPERS = ROOT / "deploy" / "scripts"
if not HELPERS.is_dir():
    HELPERS = ROOT / "network-upgrade-20261010" / "staging"


class NetworkTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        for directory in ("etc/patrol", "etc/systemd/system", "usr/bin", "run", "var/lib/patrol"):
            (self.root / directory).mkdir(parents=True)
        self.config = self.root / "etc/patrol/network.conf"
        self.password_file = self.root / "etc/patrol/wifi-password"
        self.wpa = self.root / "etc/wpa_supplicant.conf"
        self.pending = self.root / "var/lib/patrol/network-transition/pending"
        self.commands = self.root / "commands"
        self.status = self.root / "status"
        self.address = self.root / "address"
        self.old_config = "WIFI_SSID=B410\nWIFI_PASSWORD='old-private'\nSTATIC_IP=192.168.3.100\n"
        self.config.write_text(self.old_config)
        self.wpa.write_text("old-wpa-private")
        self.secret = "test-credential-$'\\\""
        self.status.write_text("wpa_state=COMPLETED\nssid=MLML\n")
        self.address.write_text("    inet 10.52.36.20/24 brd 10.52.36.255 scope global wlan0\n")
        self.environment = dict(os.environ, PATH=str(self.bin) + ":" + os.environ["PATH"],
                                NETWORK_TEST_LOG=str(self.commands), NETWORK_TEST_STATUS=str(self.status),
                                NETWORK_TEST_ADDRESS=str(self.address), NETWORK_TEST_CONFIG=str(self.config),
                                NETWORK_TEST_ARM_CONFIG=str(self.root / "arm-config"))
        common = '#!/bin/sh\nprintf "%s\\n" "$0 $*" >> "$NETWORK_TEST_LOG"\n'
        self.write_command("id", "#!/bin/sh\nprintf '0\\n'\n")
        self.write_command("systemctl", common + '''if [ "$1 $2" = 'enable --now' ]; then
    cat "$NETWORK_TEST_CONFIG" > "$NETWORK_TEST_ARM_CONFIG"
    [ "${FAIL_ARM:-0}" = 0 ] || exit 1
fi
exit 0
''')
        self.write_command("systemd-run", common + 'exit "${FAIL_SWITCH:-0}"\n')
        self.write_command("wpa_cli", common + 'cat "$NETWORK_TEST_STATUS"\n')
        self.write_command("ip", common + '''case "$*" in
    *'addr show'*) cat "$NETWORK_TEST_ADDRESS";;
    'addr replace '*) printf '    inet %s scope global %s\\n' "$3" "$5" >> "$NETWORK_TEST_ADDRESS";;
    'addr del '*) awk -v address="$3" '$2!=address' "$NETWORK_TEST_ADDRESS" > "$NETWORK_TEST_ADDRESS.tmp"; mv "$NETWORK_TEST_ADDRESS.tmp" "$NETWORK_TEST_ADDRESS";;
    *'addr flush'*) : > "$NETWORK_TEST_ADDRESS";;
    *'link show'*) exit 1;;
esac
''')
        self.write_command("sleep", "#!/bin/sh\nexit 0\n")
        self.migration = self.root / "usr/bin/patrol_network_migrate"
        content = (HELPERS / "migrate_network.sh").read_text()
        for original in ("/var/lib/patrol/network-transition", "/etc/patrol/network.conf", "/etc/patrol/wifi-password",
                         "/etc/wpa_supplicant.conf", "/etc/systemd/system", "/usr/bin/patrol_network_migrate",
                         "/usr/bin/patrol_setup_network", "/usr/bin/patrol_dhcp_hook", "/usr/bin/patrol_wifi_link_action",
                         "/run/patrol-network-migration.lock"):
            content = content.replace(original, str(self.root / original.lstrip("/")))
        self.migration.write_text(content)
        self.migration.chmod(0o755)
        for name in ("patrol_setup_network", "patrol_dhcp_hook", "patrol_wifi_link_action"):
            helper = self.root / "usr/bin" / name
            helper.write_text("#!/bin/sh\nexit 0\n")
            helper.chmod(0o755)

    def write_command(self, name, content):
        path = self.bin / name
        path.write_text(content)
        path.chmod(0o755)

    def run_migration(self, arguments, secret=None, extra_environment=None):
        environment = dict(self.environment, **(extra_environment or {}))
        result = subprocess.run(["bash", str(self.migration)] + arguments, input=secret,
                                env=environment, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertNotIn(self.secret, result.stdout + result.stderr)
        return result

    def stage(self):
        result = self.run_migration(["stage", "--ssid", "MLML", "--timeout", "300"], self.secret + "\n")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_stage_and_timeout_restore_exact_old_profile(self):
        self.stage()
        self.assertEqual(self.password_file.read_text(), self.secret + "\n")
        config = self.config.read_text()
        self.assertIn("WIFI_SSID=MLML", config)
        for secret in ("B410", "old-private", self.secret):
            self.assertNotIn(secret, config)
        self.assertEqual((self.root / "arm-config").read_text(), self.old_config)
        for path in (self.config, self.password_file):
            self.assertEqual(stat.S_IMODE(path.stat().st_mode), 0o600)
        backup = self.pending.resolve()
        self.assertEqual(stat.S_IMODE(backup.stat().st_mode), 0o700)
        commands = self.commands.read_text()
        self.assertLess(commands.index("enable --now"), commands.index("systemd-run"))
        self.assertIn("--on-active=3s", commands)
        self.assertNotIn(self.secret, commands)
        timer = self.root / "etc/systemd/system/patrol-network-rollback.timer"
        self.assertIn("OnActiveSec=300s", timer.read_text())
        result = self.run_migration(["rollback"])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.config.read_text(), self.old_config)
        self.assertEqual(self.wpa.read_text(), "old-wpa-private")
        self.assertFalse(self.password_file.exists())
        self.assertFalse(self.pending.exists())
        self.assertEqual((backup / "result").read_text(), "rolled_back\n")
        self.assertIn("restart patrol-network.service", self.commands.read_text())

    def test_confirm_requires_expected_hotspot_and_valid_dhcp_address(self):
        self.stage()
        backup = self.pending.resolve()
        self.status.write_text("wpa_state=COMPLETED\nssid=B410\n")
        self.assertNotEqual(self.run_migration(["confirm"]).returncode, 0)
        self.assertTrue(self.pending.exists())
        self.status.write_text("wpa_state=COMPLETED\nssid=MLML\n")
        self.address.write_text("    inet 169.254.1.2/16 scope global wlan0\n")
        self.assertNotEqual(self.run_migration(["confirm"]).returncode, 0)
        self.assertTrue(self.pending.exists())
        self.address.write_text("    inet 10.52.36.20/24 scope global wlan0\n")
        self.assertEqual(self.run_migration(["confirm"]).returncode, 0)
        self.assertFalse(self.pending.exists())
        self.assertEqual((backup / "result").read_text(), "confirmed\n")
        self.assertIn("disable --now patrol-network-rollback.timer", self.commands.read_text())

    def test_arming_failure_restores_config_without_scheduling_switch(self):
        result = self.run_migration(["stage"], self.secret + "\n", {"FAIL_ARM": "1"})
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.config.read_text(), self.old_config)
        self.assertFalse(self.pending.exists())
        self.assertNotIn("systemd-run", self.commands.read_text())

    def test_switch_scheduling_failure_restores_profile(self):
        result = self.run_migration(["stage"], self.secret + "\n", {"FAIL_SWITCH": "1"})
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.config.read_text(), self.old_config)
        self.assertFalse(self.password_file.exists())
        self.assertFalse(self.pending.exists())

    def test_invalid_stage_input_preserves_profile_and_apply_requires_pending(self):
        for arguments, secret in ((["stage", "--timeout", "30"], self.secret),
                                  (["stage", "--ssid", "B410"], self.secret),
                                  (["stage"], "short")):
            result = self.run_migration(arguments, secret + "\n")
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(self.config.read_text(), self.old_config)
        self.assertNotEqual(self.run_migration(["apply"]).returncode, 0)
        self.stage()
        self.assertEqual(self.run_migration(["apply"]).returncode, 0)
        self.assertNotEqual(self.run_migration(["stage"], self.secret + "\n").returncode, 0)

    def test_phone_dhcp_address_route_and_dns_and_invalid_mask(self):
        resolver = self.root / "resolv.conf"
        resolver.write_text("nameserver 192.168.3.1\n")
        hook = self.root / "dhcp_hook.sh"
        content = (HELPERS / "dhcp_hook.sh").read_text().replace("/sbin/ip", str(self.bin / "ip"))
        hook.write_text(content.replace("/etc/resolv.conf", str(resolver)))
        environment = dict(self.environment, interface="wlan0", ip="10.52.36.20", subnet="255.255.254.0",
                           router="10.52.36.194", dns="10.52.36.194 1.1.1.1")
        def execute(event):
            return subprocess.run(["sh", str(hook), event], env=environment,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        result = execute("bound")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("addr replace 10.52.36.20/23 dev wlan0", self.commands.read_text())
        self.assertIn("addr del 10.52.36.20/24 dev wlan0", self.commands.read_text())
        self.assertIn("default via 10.52.36.194 dev wlan0", self.commands.read_text())
        self.assertEqual(resolver.read_text(), "nameserver 10.52.36.194\nnameserver 1.1.1.1\n")
        self.commands.write_text("")
        self.assertEqual(execute("deconfig").returncode, 0)
        self.assertEqual(len(self.commands.read_text().splitlines()), 2)
        self.commands.write_text("")
        environment["subnet"] = "255.0.255.0"
        self.assertNotEqual(execute("bound").returncode, 0)
        self.assertEqual(self.commands.read_text(), "")
        environment["interface"] = "wlan0;injected"
        self.assertNotEqual(execute("bound").returncode, 0)
        self.assertEqual(self.commands.read_text(), "")

    def test_unchanged_dhcp_renew_keeps_address_and_default_route(self):
        resolver = self.root / "resolv.conf"
        resolver.write_text("nameserver 10.52.36.194\n")
        hook = self.root / "same-lease-hook.sh"
        content = (HELPERS / "dhcp_hook.sh").read_text().replace("/sbin/ip", str(self.bin / "ip"))
        hook.write_text(content.replace("/etc/resolv.conf", str(resolver)))
        environment = dict(self.environment, interface="wlan0", ip="10.52.36.20", subnet="255.255.255.0",
                           router="10.52.36.194", dns="10.52.36.194")
        original_address = self.address.read_text()
        for event in ("bound", "renew", "renew"):
            result = subprocess.run(["sh", str(hook), event], env=environment,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(self.address.read_text(), original_address)
        commands = self.commands.read_text()
        for mutation in ("addr flush", "addr del", "addr add", "addr replace", "route flush"):
            self.assertNotIn(mutation, commands)
        self.assertEqual(commands.count("route replace default via 10.52.36.194"), 3)
        self.assertEqual(resolver.read_text(), "nameserver 10.52.36.194\n")

    def test_hotspot_subnet_change_adds_new_address_before_removing_old(self):
        resolver = self.root / "resolv.conf"
        resolver.write_text("nameserver 192.168.3.1\n")
        self.address.write_text("    inet 192.168.3.112/24 scope global wlan0\n")
        hook = self.root / "subnet-change-hook.sh"
        content = (HELPERS / "dhcp_hook.sh").read_text().replace("/sbin/ip", str(self.bin / "ip"))
        hook.write_text(content.replace("/etc/resolv.conf", str(resolver)))
        environment = dict(self.environment, interface="wlan0", ip="10.52.36.20", subnet="255.255.255.0",
                           router="10.52.36.194", dns="10.52.36.194")
        result = subprocess.run(["sh", str(hook), "bound"], env=environment,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        commands = self.commands.read_text()
        self.assertLess(commands.index("addr replace 10.52.36.20/24"), commands.index("addr del 192.168.3.112/24"))
        self.assertNotIn("addr flush", commands)
        self.assertNotIn("route flush", commands)
        self.assertIn("inet 10.52.36.20/24", self.address.read_text())
        self.assertNotIn("192.168.3.112", self.address.read_text())
        self.assertEqual(resolver.read_text(), "nameserver 10.52.36.194\n")

    def test_setup_password_escaping_file_protection_and_missing_secret(self):
        generated = self.root / "generated-wpa.conf"
        secret_file = self.root / "secret"
        secret_file.write_text(self.secret + "\n")
        secret_file.chmod(0o644)
        configuration = self.root / "setup.conf"
        configuration.write_text("WIFI_SSID=MLML\nWPA_CONF=%s\nWIFI_PASSWORD_FILE=%s\n" % (generated, secret_file))
        result = subprocess.run(["bash", str(HELPERS / "setup_network.sh"), str(configuration)],
                                env=self.environment, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(result.returncode, 1)  # Fake interface never appears.
        expected = self.secret.replace("\\", "\\\\").replace('"', '\\"')
        self.assertIn('psk="%s"' % expected, generated.read_text())
        self.assertNotIn(self.secret, result.stdout + result.stderr)
        self.assertEqual(stat.S_IMODE(generated.stat().st_mode), 0o600)
        self.assertEqual(stat.S_IMODE(secret_file.stat().st_mode), 0o600)
        generated.write_text("keep-existing-private-config")
        secret_file.unlink()
        result = subprocess.run(["bash", str(HELPERS / "setup_network.sh"), str(configuration)],
                                env=self.environment, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(generated.read_text(), "keep-existing-private-config")


if __name__ == "__main__":
    unittest.main(verbosity=2)
