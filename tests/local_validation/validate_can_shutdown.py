"""Run the actual shutdown helper against isolated USB/CAN fixtures."""
from pathlib import Path
import os
import subprocess
import tempfile
import sys

repo = Path(sys.argv[1]).resolve()
source = (repo/'deploy/scripts/loong-usb-can-down.sh').read_text()
cases = ['running', 'normal', 'missing', 'two_adapters', 'two_bindings',
         'wrong_vendor', 'wrong_type', 'invalid_name', 'other_interface']
for case in cases:
    with tempfile.TemporaryDirectory(prefix='can-shutdown-') as directory:
        root = Path(directory)
        for path in ['bin', 'run', 'proc', 'sys/class/net/can2',
                     'sys/bus/usb/drivers/pcan', 'sys/bus/usb/devices/3-1/3-1:1.0']:
            (root/path).mkdir(parents=True, exist_ok=True)
        driver = root/'sys/bus/usb/drivers/pcan'
        (driver/'3-1:1.0').symlink_to(root/'sys/bus/usb/devices/3-1/3-1:1.0')
        (driver/'unbind').write_text('')
        (root/'sys/class/net/can2/type').write_text('280\n')
        (root/'sys/bus/usb/devices/3-1/idVendor').write_text('0c72\n')
        (root/'sys/bus/usb/devices/3-1/idProduct').write_text('000c\n')
        (root/'proc/pcan').write_text('32 usb can2 0000 0000\n')
        if case == 'missing':
            (root/'proc/pcan').unlink()
        if case == 'two_adapters':
            (root/'proc/pcan').write_text('32 usb can2 0 0\n33 usb can4 0 0\n')
        if case == 'two_bindings':
            (driver/'3-2:1.0').symlink_to(root/'sys/bus/usb/devices/3-1/3-1:1.0')
        if case == 'wrong_vendor':
            (root/'sys/bus/usb/devices/3-1/idVendor').write_text('1234\n')
        if case == 'wrong_type':
            (root/'sys/class/net/can2/type').write_text('1\n')
        if case == 'other_interface':
            (root/'proc/pcan').write_text('32 usb can0 0 0\n')
        for name, text in {
            'systemctl':'#!/bin/sh\nprintf "%s\\n" "$MOCK_STATE"\n',
            'ip':'#!/bin/sh\nprintf "%s\\n" "$*" >> "$MOCK_LOG"\n'
        }.items():
            path = root/'bin'/name
            path.write_text(text)
            path.chmod(0o755)
        text = source.replace('PATH=/usr/sbin:/usr/bin:/sbin:/bin',
                              'PATH='+str(root/'bin')+':/usr/bin:/bin')
        for old, new in [('/sys/', str(root/'sys')+'/'),
                         ('/proc/pcan', str(root/'proc/pcan')),
                         ('/run/', str(root/'run')+'/')]:
            text = text.replace(old, new)
        script = root/'shutdown.sh'
        script.write_text(text)
        environment = dict(os.environ, MOCK_STATE='running' if case=='running' else 'stopping',
                           MOCK_LOG=str(root/'ip.log'))
        result = subprocess.run(['sh', str(script), 'eth0' if case=='invalid_name' else 'can2'],
                                env=environment, capture_output=True, text=True, timeout=5)
        assert result.returncode == (2 if case=='invalid_name' else 0), (case, result.stderr)
        if case == 'normal':
            assert (root/'ip.log').read_text() == 'link set dev can2 down\n'
            assert (driver/'unbind').read_text() == '3-1:1.0\n'
        else:
            assert not (root/'ip.log').exists(), case
            assert (driver/'unbind').read_text() == '', case
print('PASS: shutdown-only PCAN release; running/absent/ambiguous/wrong-identity/native-interface cases leave hardware untouched')
