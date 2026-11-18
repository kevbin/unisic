#!/usr/bin/env python3
"""Publish only complete, runnable bundles after both build workflows pass."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess


def run(*args, **kwargs):
    result = subprocess.run(args, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, **kwargs)
    if result.returncode:
        raise RuntimeError(f'{args[0]} exited {result.returncode}: {result.stderr.strip()}')
    return result.stdout.strip()


def publish():
    version = os.environ['FORK_VERSION']
    source = os.environ['SOURCE_SHA']
    repository = os.environ['GITHUB_REPOSITORY']
    if repository != 'kevbin/unisic' or not re.fullmatch(r'\d+\.\d+\.\d+\.[1-9]\d*', version):
        raise RuntimeError('Invalid personal release channel/version')
    if not re.fullmatch(r'[0-9a-f]{40}', source):
        raise RuntimeError('Release must target an exact source commit')
    dist = Path('dist')
    image = dist / f'Unisic-{version}-x86_64.AppImage'
    zsync = image.with_suffix('.AppImage.zsync')
    tarball = dist / f'unisic-{version}-x86_64.tar.gz'
    assets = [image, zsync, tarball]
    for asset in assets:
        if not asset.is_file() or asset.stat().st_size == 0:
            raise RuntimeError(f'Missing release asset: {asset}')
    image.chmod(0o755)
    # FUSE-free runtime verification on the same Ubuntu version as the user.
    actual = run(str(image.resolve()), '--appimage-extract-and-run', '--version', timeout=120)
    if actual != version:
        raise RuntimeError(f'Bundle version {actual!r} does not match release {version}')
    zsync_header = zsync.read_bytes().split(b'\n\n', 1)[0].decode('ascii')
    if f'Filename: {image.name}' not in zsync_header or f'URL: {image.name}' not in zsync_header:
        raise RuntimeError('Differential-update metadata names the wrong AppImage')
    checksums = dist / 'SHA256SUMS'
    sums = []
    for asset in assets:
        with asset.open('rb') as stream:
            digest = hashlib.file_digest(stream, 'sha256').hexdigest()
        sums.append(f'{digest}  {asset.name}\n')
    checksums.write_text(''.join(sums))
    provenance = dist / 'build-info.json'
    provenance.write_text(json.dumps({
        'repository': repository, 'version': version, 'source_sha': source,
        'upstream_tag': os.environ['UPSTREAM_TAG'],
        'upstream_sha': os.environ['UPSTREAM_SHA'],
        'workflow_run': f"https://github.com/{repository}/actions/runs/{os.environ['GITHUB_RUN_ID']}",
        'patches': ['Simplified Chinese interface', 'GNOME scrolling capture black-screen fix'],
    }, indent=2) + '\n')
    notes = Path('fork-release-notes.md')
    notes.write_text(f'''基于上游 {os.environ['UPSTREAM_TAG']} 的个人维护版，保留简体中文界面和 GNOME 滚动截图黑屏修复。

- 通过完整 QtTest 测试、翻译覆盖与占位符检查，以及 AppImage 启动校验。
- 更新来源为 `{repository}`；未来新版由 GitHub Actions 检查、合并、测试后发布。
- 上游合并冲突或新增文本尚未翻译时停止发布，保留上一可用版本。
- Ubuntu 24.04 或更新版本可使用 AppImage，也可解压便携 tar.gz 后运行 AppRun。

Source: `{source}`
Upstream: `{os.environ['UPSTREAM_SHA']}`
Build: https://github.com/{repository}/actions/runs/{os.environ['GITHUB_RUN_ID']}

This is a personal fork, not an official Unisic release. SHA256SUMS and build-info.json accompany the bundles.
''')
    tag = f'v{version}'
    existing = subprocess.run(['gh', 'release', 'view', tag, '--repo', repository,
                               '--json', 'isDraft'], text=True, capture_output=True)
    if existing.returncode == 0:
        if not json.loads(existing.stdout)['isDraft']:
            raise RuntimeError('Refusing to overwrite an already published release')
        tagged_source = run('gh', 'api', f'repos/{repository}/commits/{tag}', '--jq', '.sha')
        if tagged_source != source:
            raise RuntimeError('Draft tag targets a different source commit')
    else:
        run('gh', 'release', 'create', tag, '--repo', repository, '--target', source,
            '--draft', '--title', f'Unisic {version} 简体中文维护版', '--notes-file', str(notes))
    run('gh', 'release', 'upload', tag, '--repo', repository, '--clobber',
        *map(str, assets + [checksums, provenance]))
    run('gh', 'release', 'edit', tag, '--repo', repository, '--draft=false', '--latest')
    print(f'Published https://github.com/{repository}/releases/tag/{tag}')


if __name__ == '__main__':
    publish()
