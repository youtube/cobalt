from unittest import TestCase
import tempfile
import shutil
import json
import mock
import os

from clobberer import do_clobber
from clobberer import (
    get_clobber_times,
    legacy_get_clobber_times,
    make_argparser,
    process_clobber_times
)


class TestClobbererClient(TestCase):
    def setUp(self):
        self.outer_dir = tempfile.mkdtemp()
        os.chdir(self.outer_dir)  # replicating client logic

        self.slave_name = 'un-petit-bebe'
        parser = make_argparser()
        self.process_args, legacy = parser.parse_args([
            '--dir',
            '/',
            '--slave',
            self.slave_name
        ])

    def tearDown(self):
        shutil.rmtree(self.outer_dir)

    def gen_server_clobber_times(self, slave_name=None):
        return {'result': [
            {
                'builddir': 'tmp',
                'lastclobber': 1,
                'who': 'albert.camus@gmail.net',
                'slave': slave_name
            }
        ]}

    def test_do_clobber(self):
        file_name = 'my-name-is-mud'
        inner_dir = tempfile.mkdtemp(dir=self.outer_dir)
        open(os.path.join(self.outer_dir, file_name), 'a').close()
        self.assertTrue(os.path.exists(self.outer_dir))
        self.assertTrue(os.path.exists(inner_dir))
        self.assertTrue(os.path.exists(file_name))
        do_clobber(self.outer_dir)
        self.assertFalse(os.path.exists(inner_dir))
        self.assertFalse(os.path.exists(file_name))
        self.assertTrue(os.path.exists(self.outer_dir))

    def test_do_clobber_with_skip(self):
        skip_dir_name = 'muddy-mud-skipper'
        skip_file_name = 'powdered-toast'
        inner_dir = tempfile.mkdtemp(dir=self.outer_dir)
        open(os.path.join(self.outer_dir, skip_file_name), 'a').close()
        os.mkdir(os.path.join(self.outer_dir, skip_dir_name))
        self.assertTrue(os.path.exists(inner_dir))
        self.assertTrue(os.path.exists(skip_dir_name))
        self.assertTrue(os.path.exists(skip_file_name))
        do_clobber(self.outer_dir, skip=[skip_dir_name, skip_file_name])
        self.assertFalse(os.path.exists(inner_dir))
        self.assertTrue(os.path.exists(skip_dir_name))
        self.assertTrue(os.path.exists(skip_file_name))
        os.rmdir(skip_dir_name)
        os.remove(skip_file_name)

    @mock.patch('urllib2.urlopen')
    def test_get_clobber_times(self, mock_urlopen):
        builddir = 'the-roadhouse'
        timestamp = 9999
        who = 'JimMorrison@thedoors.net'
        fake_data = {'result': [
            {
                'builddir': builddir,
                'lastclobber': timestamp,
                'who': who,
                'slave': None
                }
            ]
        }
        fake_data_json_str = json.dumps(fake_data)
        urllib2_mock = mock.Mock()
        urllib2_mock.read.side_effect = [fake_data_json_str]
        mock_urlopen.return_value = urllib2_mock

        clobber_dates_return = get_clobber_times('clobberer/lastclobber/all')
        self.assertDictEqual(clobber_dates_return, fake_data)

    @mock.patch('urllib2.urlopen')
    def test_get_clobber_times_null(self, mock_urlopen):
        # make sure it can handle no data
        fake_data_null = {'result': []}
        fake_data_json_str = json.dumps(fake_data_null)
        urllib2_mock = mock.Mock()
        urllib2_mock.read.side_effect = [fake_data_json_str]
        mock_urlopen.return_value = urllib2_mock

        clobber_dates_return = get_clobber_times('clobberer/lastclobber/all')
        self.assertDictEqual(clobber_dates_return, fake_data_null)

    @mock.patch('urllib2.urlopen')
    def test_legacy_get_clobber_times(self, mock_urlopen):
        lastclobber_fmt = '{}:{}:{}\n'
        builddir = 'the-roadhouse'
        timestamp = 9999
        who = 'JimMorrison@thedoors.net'

        fake_data_str = lastclobber_fmt.format(builddir, timestamp, who)
        urllib2_mock = mock.Mock()
        urllib2_mock.read.side_effect = [fake_data_str]
        mock_urlopen.return_value = urllib2_mock

        clobber_dates_return = legacy_get_clobber_times(
            'clobberer/lastclobber',
            'branch',
            'buildername',
            'builddir',
            'slave',
            'master',
        )

        self.assertDictEqual(
            clobber_dates_return,
            {
                'result': [{
                    'builddir': builddir,
                    'lastclobber': timestamp,
                    'who': who,
                    'slave': None
                }]
            }
        )

    @mock.patch('urllib2.urlopen')
    def test_legacy_get_clobber_times_null(self, mock_urlopen):
        # make sure it can handle no data
        fake_data_str = ""
        urllib2_mock = mock.Mock()
        urllib2_mock.read.side_effect = [fake_data_str]
        mock_urlopen.return_value = urllib2_mock

        clobber_dates_return = legacy_get_clobber_times(
            'clobberer/lastclobber',
            'branch',
            'buildername',
            'builddir',
            'slave',
            'master',
        )
        self.assertDictEqual(clobber_dates_return, {'result': []})

    @mock.patch('clobberer.write_file')
    @mock.patch('clobberer.do_clobber')
    def test_process_clobber_times_no_slave(self, mock_do_clobber, mock_write_file):
        server_clobber_times_no_slave = self.gen_server_clobber_times()
        # so we can tell if a clobber has been triggered
        process_clobber_times(server_clobber_times_no_slave, self.process_args)
        mock_do_clobber.assert_called_with('/tmp', False, ['last-clobber'])

    @mock.patch('clobberer.write_file')
    @mock.patch('clobberer.do_clobber')
    def test_process_clobber_times_wrong_slave(self, mock_do_clobber, mock_write_file):
        # should cause no response
        server_clobber_times_wrong_slave = self.gen_server_clobber_times('iamamwhoami')
        process_clobber_times(server_clobber_times_wrong_slave, self.process_args)
        assert not mock_do_clobber.called

    @mock.patch('clobberer.write_file')
    @mock.patch('clobberer.do_clobber')
    def test_process_clobber_times_right_slave(self, mock_do_clobber, mock_write_file):
        server_clobber_times_right_slave = self.gen_server_clobber_times(self.slave_name)
        process_clobber_times(server_clobber_times_right_slave, self.process_args)
        mock_do_clobber.assert_called_with('/tmp', False, ['last-clobber'])

if __name__ == '__main__':
    import unittest
    unittest.main()
