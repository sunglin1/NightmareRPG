# Sends a Python file to the running Unreal Editor via Remote Execution and prints the output.
# Usage: python ue_run.py <script.py>
import sys
import time

sys.path.insert(0, r"C:\Program Files\Epic Games\UE_5.8\Engine\Plugins\Experimental\PythonScriptPlugin\Content\Python")
import remote_execution as re_  # noqa: E402

import os  # noqa: E402
code = os.path.abspath(sys.argv[1]).replace("\\", "/")  # ExecuteFile accepts a file path
rx = re_.RemoteExecution()
rx.start()
try:
    for _ in range(30):
        if rx.remote_nodes:
            break
        time.sleep(0.2)
    if not rx.remote_nodes:
        print("NO UNREAL NODE FOUND (is Remote Execution enabled and editor open?)")
        sys.exit(2)
    rx.open_command_connection(rx.remote_nodes[0]["node_id"])
    res = rx.run_command(code, unattended=True, exec_mode=re_.MODE_EXEC_FILE)
    for o in res.get("output", []):
        print(o.get("output", "").rstrip())
    print("SUCCESS" if res.get("success") else "FAILED: " + str(res.get("result")))
finally:
    rx.stop()
