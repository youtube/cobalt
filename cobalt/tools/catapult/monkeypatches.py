# Import the function to be used as a patch
from cobalt.tools.catapult.monkeypatch.version_number import MonkeyPatchVersionNumber

# Import the class to be patched
from telemetry.internal.backends.chrome_inspector import devtools_client_backend

# Apply the monkeypatch
devtools_client_backend._DevToolsClientBackend.GetChromeMajorNumber = MonkeyPatchVersionNumber
