"""Offline regressions for automatic upstream merges and release decisions."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import os
from unittest.mock import patch
import unittest

spec = importlib.util.spec_from_file_location('fork_release', Path(__file__).resolve().parents[1] / 'scripts/fork_release.py')
fork = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fork)


class ForkReleaseTest(unittest.TestCase):
    def test_versions(self):
        self.assertEqual(fork.release_version('0.8.9', 12), '0.8.9.12')
        for base, number in [('0.8.9b', 1), ('0.8.9', 0), ('latest', 1), ('0.8.9', '1\n2')]:
            with self.assertRaises(ValueError):
                fork.release_version(base, number)

    def test_duplicate_release(self):
        self.assertFalse(fork.should_release({'target_commitish': 'abc'}, 'abc'))
        self.assertTrue(fork.should_release({'target_commitish': 'abc'}, 'def'))
        self.assertTrue(fork.should_release({'target_commitish': 'abc'}, 'abc', True))
        self.assertTrue(fork.should_release(None, 'abc'))

    def test_complete_chinese_catalog(self):
        self.assertGreater(fork.validate_catalog(fork.ROOT), 1800)

    def init_repo(self, path):
        fork.git(path, 'init', '-b', 'main')
        fork.git(path, 'config', 'user.name', 'Offline Test')
        fork.git(path, 'config', 'user.email', 'test@example.invalid')
        (path / '.github/workflows').mkdir(parents=True)
        (path / '.github/workflows/release.yml').write_text('original\n')
        (path / 'source.txt').write_text('original source\n')
        fork.git(path, 'add', '.')
        fork.git(path, 'commit', '-m', 'base')
        fork.git(path, 'branch', 'upstream')

    def commit(self, path, message):
        fork.git(path, 'add', '.')
        fork.git(path, 'commit', '-m', message)

    def test_merge_keeps_patches_and_workflows(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.init_repo(root)
            (root / 'chinese.ts').write_text('Chinese patch\n')
            (root / 'scroll-fix.cpp').write_text('GNOME patch\n')
            (root / '.github/workflows/release.yml').write_text('personal workflow\n')
            self.commit(root, 'personal fixes')
            fork.git(root, 'checkout', 'upstream')
            (root / 'source.txt').write_text('new upstream source\n')
            (root / '.github/workflows/release.yml').write_text('new upstream workflow\n')
            (root / '.github/workflows/new.yml').write_text('upstream only\n')
            self.commit(root, 'upstream update')
            fork.git(root, 'checkout', 'main')
            self.assertTrue(fork.merge_upstream(root, 'upstream'))
            self.assertEqual((root / 'source.txt').read_text(), 'new upstream source\n')
            self.assertEqual((root / '.github/workflows/release.yml').read_text(), 'personal workflow\n')
            self.assertFalse((root / '.github/workflows/new.yml').exists())
            self.assertTrue((root / 'chinese.ts').exists())
            self.assertTrue((root / 'scroll-fix.cpp').exists())
            self.commit(root, 'merge')
            self.assertFalse(fork.merge_upstream(root, 'upstream'))

    def test_source_conflict_aborts_without_overwriting(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.init_repo(root)
            (root / 'source.txt').write_text('personal change\n')
            self.commit(root, 'personal')
            old_head = fork.git(root, 'rev-parse', 'HEAD').stdout
            fork.git(root, 'checkout', 'upstream')
            (root / 'source.txt').write_text('conflicting upstream change\n')
            self.commit(root, 'upstream')
            fork.git(root, 'checkout', 'main')
            with self.assertRaisesRegex(RuntimeError, 'source.txt'):
                fork.merge_upstream(root, 'upstream')
            self.assertEqual(fork.git(root, 'rev-parse', 'HEAD').stdout, old_head)
            self.assertEqual((root / 'source.txt').read_text(), 'personal change\n')
            self.assertEqual(fork.git(root, 'status', '--porcelain').stdout, '')


class PublishGateTest(unittest.TestCase):
    def setUp(self):
        pubspec = importlib.util.spec_from_file_location('publisher', fork.ROOT / 'scripts/publish_fork_release.py')
        self.publisher = importlib.util.module_from_spec(pubspec)
        pubspec.loader.exec_module(self.publisher)
        self.tmp = tempfile.TemporaryDirectory()
        self.old_cwd = Path.cwd()
        os.chdir(self.tmp.name)
        Path('dist').mkdir()
        self.env = patch.dict(os.environ, {
            'FORK_VERSION': '0.8.9.1', 'SOURCE_SHA': 'a' * 40,
            'GITHUB_REPOSITORY': 'kevbin/unisic', 'GITHUB_RUN_ID': '1',
            'UPSTREAM_TAG': 'v0.8.9', 'UPSTREAM_SHA': 'b' * 40,
        })
        self.env.start()

    def tearDown(self):
        self.env.stop()
        os.chdir(self.old_cwd)
        self.tmp.cleanup()

    def bundle(self):
        Path('dist/Unisic-0.8.9.1-x86_64.AppImage').write_bytes(b'fake runtime')
        Path('dist/unisic-0.8.9.1-x86_64.tar.gz').write_bytes(b'fake tarball')
        Path('dist/Unisic-0.8.9.1-x86_64.AppImage.zsync').write_text(
            'Filename: Unisic-0.8.9.1-x86_64.AppImage\nURL: Unisic-0.8.9.1-x86_64.AppImage\n\n')

    def test_missing_bundle_does_not_publish(self):
        with patch.object(self.publisher, 'run') as commands:
            with self.assertRaisesRegex(RuntimeError, 'Missing release asset'):
                self.publisher.publish()
            commands.assert_not_called()

    def test_wrong_bundle_version_does_not_publish(self):
        self.bundle()
        with patch.object(self.publisher, 'run', return_value='0.8.9') as commands:
            with self.assertRaisesRegex(RuntimeError, 'does not match'):
                self.publisher.publish()
            self.assertEqual(commands.call_count, 1)

    def test_upload_failure_leaves_release_draft(self):
        self.bundle()
        calls = []
        def command(*args, **kwargs):
            calls.append(args)
            if args[0].endswith('.AppImage'):
                return '0.8.9.1'
            if args[:3] == ('gh', 'release', 'upload'):
                raise RuntimeError('upload interrupted')
            return ''
        no_release = subprocess.CompletedProcess([], 1, '', 'HTTP 404')
        with patch.object(self.publisher, 'run', side_effect=command), patch.object(self.publisher.subprocess, 'run', return_value=no_release):
            with self.assertRaisesRegex(RuntimeError, 'upload interrupted'):
                self.publisher.publish()
        self.assertTrue(any(c[:3] == ('gh', 'release', 'create') and '--draft' in c for c in calls))
        self.assertFalse(any(c[:3] == ('gh', 'release', 'edit') for c in calls))
        self.assertIn('Unisic-0.8.9.1-x86_64.AppImage', Path('dist/SHA256SUMS').read_text())


if __name__ == '__main__':
    unittest.main()
