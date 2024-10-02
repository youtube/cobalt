#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys

sys.path.append(os.path.dirname(__file__))

from signing import config_factory, commands, logger, model, pipeline


def create_config(config_args, development):
    """Creates the |model.CodeSignConfig| for the signing operations.

    If |development| is True, the config will be modified to not require
    restricted internal assets, nor will the products be required to match
    specific certificate hashes.

    Args:
        config_args: List of args to expand to the config class's constructor.
        development: Boolean indicating whether or not to modify the chosen
            config for development testing.

    Returns:
        An instance of |model.CodeSignConfig|.
    """
    config_class = config_factory.get_class()

    if development:

        class DevelopmentCodeSignConfig(config_class):

            @property
            def codesign_requirements_basic(self):
                return ''

            @property
            def provisioning_profile_basename(self):
                return None

            @property
            def run_spctl_assess(self):
                # Self-signed or ad-hoc signed signing identities won't pass
                # spctl assessment so don't do it.
                return False

            @property
            def inject_get_task_allow_entitlement(self):
                return True

        config_class = DevelopmentCodeSignConfig

    return config_class(**config_args)


def _show_tool_versions():
    logger.info('Showing macOS and tool versions.')
    commands.run_command(['sw_vers'])
    commands.run_command(['xcodebuild', '-version'])
    commands.run_command(['xcrun', '-show-sdk-path'])


def main():
    parser = argparse.ArgumentParser(
        description='Code sign and package Chrome for channel distribution.')
    parser.add_argument(
        '--identity',
        required=True,
        help='The identity to sign everything but PKGs with.')
    parser.add_argument(
        '--installer-identity', help='The identity to sign PKGs with.')
    parser.add_argument(
        '--notary-user',
        help='The username used to authenticate to the Apple notary service.')
    parser.add_argument(
        '--notary-password',
        help='The password or password reference (e.g. @keychain, see '
        '`xcrun altool -h`) used to authenticate to the Apple notary service.')
    parser.add_argument(
        '--notary-asc-provider',
        help='The ASC provider string to be used as the `--asc-provider` '
        'argument to `xcrun altool`, to be used when --notary-user is '
        'associated with multiple Apple developer teams. See `xcrun altool -h. '
        'Run `iTMSTransporter -m provider -account_type itunes_connect -v off '
        '-u USERNAME -p PASSWORD` to list valid providers.')
    parser.add_argument(
        '--notary-team-id',
        help='The Apple Developer Team ID used to authenticate to the Apple '
        'notary service.')
    parser.add_argument(
        '--notarization-tool',
        choices=list(model.NotarizationTool),
        type=model.NotarizationTool,
        default=None,
        help='The tool to use to communicate with the Apple notary service.')
    parser.add_argument(
        '--development',
        action='store_true',
        help='The specified identity is for development. Certain codesign '
        'requirements will be omitted.')
    parser.add_argument(
        '--input',
        required=True,
        help='Path to the input directory. The input directory should '
        'contain the products to sign, as well as the Packaging directory.')
    parser.add_argument(
        '--output',
        required=True,
        help='Path to the output directory. The signed (possibly packaged) '
        'products and installer tools will be placed here.')
    parser.add_argument(
        '--disable-packaging',
        action='store_true',
        help='Disable creating any packaging (.dmg/.pkg) specified by the '
        'configuration.')
    parser.add_argument(
        '--skip-brand',
        dest='skip_brands',
        action='append',
        default=[],
        help='Causes any distribution whose brand code matches to be skipped. '
        'A value of * matches all brand codes.')
    parser.add_argument(
        '--channel',
        dest='channels',
        action='append',
        default=[],
        help='If provided, only the distributions matching the specified '
        'channel(s) will be produced. The string "stable" matches the None '
        'channel.')
    parser.add_argument(
        '--notarize',
        nargs='?',
        choices=model.NotarizeAndStapleLevel.valid_strings(),
        const='staple',
        default='none',
        help='Specifies the requested notarization actions to be taken. '
        '`none` causes no notarization tasks to be performed. '
        '`nowait` submits the signed application and packaging to Apple for '
        'notarization, but does not wait for a reply. '
        '`wait-nostaple` submits the signed application and packaging to Apple '
        'for notarization, and waits for a reply, but does not staple the '
        'resulting notarization ticket. '
        '`staple` submits the signed application and packaging to Apple for '
        'notarization, waits for a reply, and staples the resulting '
        'notarization ticket. '
        'If the `--notarize` argument is not present, that is the equivalent '
        'of `--notarize none`. If the `--notarize` argument is present but '
        'has no option specified, that is the equivalent of `--notarize '
        'staple`.')

    args = parser.parse_args()

    notarization = model.NotarizeAndStapleLevel.from_string(args.notarize)
    if notarization.should_notarize():
        if not args.notary_user or not args.notary_password:
            parser.error('The `--notary-user` and `--notary-password` '
                         'arguments are required if notarizing.')

    config = create_config(
        model.pick(args, (
            'identity',
            'installer_identity',
            'notary_user',
            'notary_password',
            'notary_asc_provider',
            'notary_team_id',
            'notarization_tool',
        )), args.development)

    if config.notarization_tool == model.NotarizationTool.NOTARYTOOL:
        # Let the config override notary_team_id, including a potentially
        # unspecified argument.
        if not config.notary_team_id:
            parser.error('The `--notarization-tool=notarytool` option requires '
                         'a --notary-team-id.')

    paths = model.Paths(args.input, args.output, None)

    if not os.path.exists(paths.output):
        os.mkdir(paths.output)

    _show_tool_versions()

    pipeline.sign_all(
        paths,
        config,
        disable_packaging=args.disable_packaging,
        notarization=notarization,
        skip_brands=args.skip_brands,
        channels=args.channels)


if __name__ == '__main__':
    main()
