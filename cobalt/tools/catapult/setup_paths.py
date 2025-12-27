import os
import sys

# Add the project root to the path to allow absolute imports
REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))
sys.path.insert(0, REPOSITORY_ROOT)

# Add paths to devil and telemetry
sys.path.append(
    os.path.join(REPOSITORY_ROOT, 'third_party', 'catapult', 'devil'))
sys.path.append(
    os.path.join(REPOSITORY_ROOT, 'third_party', 'catapult', 'telemetry'))
sys.path.append(os.path.join(REPOSITORY_ROOT, 'build', 'android'))
