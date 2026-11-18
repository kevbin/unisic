#!/usr/bin/env python3
"""Prepare a tested-release candidate without discarding personal patches."""
import argparse
from collections import Counter
import json
import os
from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
TOKEN = re.compile(r'%L?\d+|%n|%[A-Za-z_][A-Za-z0-9_]*%|\*\.[A-Za-z0-9]+|\$[^$\s]+\$|\$(?:input|output|USER)\b')


def git(root, *args, check=True):
    return subprocess.run(['git', '-C', str(root), *args], check=check,
                          text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def release_version(base, number):
    if not re.fullmatch(r'\d+\.\d+\.\d+', base):
        raise ValueError('Only stable three-component upstream versions are supported')
    if not re.fullmatch(r'[1-9]\d*', str(number)):
        raise ValueError('The fork revision must be a positive workflow run number')
    return f'{base}.{number}'


def merge_upstream(root, revision):
    """Keep fork-owned workflows; source conflicts leave HEAD untouched."""
    if git(root, 'status', '--porcelain').stdout.strip():
        raise RuntimeError('Refusing to merge into a dirty checkout')
    if git(root, 'merge-base', '--is-ancestor', revision, 'HEAD', check=False).returncode == 0:
        return False
    try:
        result = git(root, 'merge', '--no-ff', '--no-commit', revision, check=False)
        conflicts = git(root, 'diff', '--name-only', '--diff-filter=U').stdout.splitlines()
        source_conflicts = [p for p in conflicts if not p.startswith('.github/workflows/')]
        if source_conflicts or (result.returncode and not conflicts):
            raise RuntimeError('Upstream merge needs manual resolution: ' +
                               (', '.join(source_conflicts) or result.stderr))
        # A future upstream workflow must not replace this fork's release gates
        # or publish an official-version asset on the personal update channel.
        git(root, 'restore', '--source=HEAD', '--staged', '--worktree', '.github/workflows')
        return True
    except Exception:
        git(root, 'merge', '--abort', check=False)
        raise


def validate_catalog(root):
    def messages(language):
        tree = ET.parse(root / f'i18n/unisic_{language}.ts')
        return {(c.findtext('name'), m.findtext('source'), m.findtext('comment', '')): m
                for c in tree.getroot().findall('context') for m in c.findall('message')
                if m.find('translation').get('type') not in ('obsolete', 'vanished')}
    english, chinese = messages('en'), messages('zh_CN')
    missing = english.keys() - chinese.keys()
    if missing:
        raise RuntimeError(f'{len(missing)} upstream texts need Chinese translation: {next(iter(missing))}')
    for key in english:
        translation = chinese[key].find('translation')
        if translation.get('type') == 'unfinished':
            raise RuntimeError(f'Unfinished Chinese translation: {key}')
        forms = translation.findall('numerusform') or [translation]
        for form in forms:
            text = ''.join(form.itertext())
            if not text.strip() or Counter(TOKEN.findall(key[1])) != Counter(TOKEN.findall(text)):
                raise RuntimeError(f'Empty translation or mismatched placeholders: {key}')
    return len(english)


def api(path):
    return json.loads(subprocess.check_output(['gh', 'api', path], text=True))


def should_release(latest, source_sha, force=False):
    return force or latest is None or latest.get('target_commitish') != source_sha


def prepare(force=False):
    repository = os.environ['GITHUB_REPOSITORY']
    if repository != 'kevbin/unisic':
        raise RuntimeError('This personal release channel belongs to kevbin/unisic')
    upstream = api('repos/unisic/unisic/releases/latest')
    tag = upstream['tag_name']
    if upstream.get('prerelease') or upstream.get('draft') or not re.fullmatch(r'v?\d+\.\d+\.\d+', tag):
        raise RuntimeError(f'Unsupported upstream release: {tag}')
    git(ROOT, 'fetch', '--no-tags', 'https://github.com/unisic/unisic.git', f'refs/tags/{tag}')
    upstream_sha = git(ROOT, 'rev-parse', 'FETCH_HEAD^{commit}').stdout.strip()
    merged = merge_upstream(ROOT, upstream_sha)
    try:
        count = validate_catalog(ROOT)
        content = (ROOT / 'CMakeLists.txt').read_text()
        match = re.search(r'^set\(UNISIC_VERSION_STRING "([^"]+)"\)', content, re.M)
        if not match or match[1] != tag.removeprefix('v'):
            raise RuntimeError('Merged CMake version does not match the stable upstream tag')
        version = release_version(match[1], os.environ['GITHUB_RUN_NUMBER'])
        if merged:
            git(ROOT, 'commit', '-m', f'Merge upstream release {tag}; preserve personal workflows')
            git(ROOT, 'push', 'origin', 'HEAD:main')
    except Exception:
        if merged:
            git(ROOT, 'merge', '--abort', check=False)
        raise
    source_sha = git(ROOT, 'rev-parse', 'HEAD').stdout.strip()
    releases = api(f'repos/{repository}/releases?per_page=100')
    latest = next((r for r in releases if not r['draft'] and not r['prerelease']), None)
    outputs = {'should_release': str(should_release(latest, source_sha, force)).lower(),
               'source_sha': source_sha, 'version': version,
               'upstream_tag': tag, 'upstream_sha': upstream_sha}
    with open(os.environ['GITHUB_OUTPUT'], 'a') as stream:
        for key, value in outputs.items():
            stream.write(f'{key}={value}\n')
    print(json.dumps({**outputs, 'translated_messages': count}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--force', action='store_true')
    parser.add_argument('--check-catalog', action='store_true')
    args = parser.parse_args()
    if args.check_catalog:
        print(f'{validate_catalog(ROOT)} active Chinese translations passed coverage/token checks')
    else:
        prepare(args.force)
