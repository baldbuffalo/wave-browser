#!/bin/bash
# Attribute commits to the GitHub account behind the session token, instead of
# the generic "openhands" identity. GitHub links commits by email, so without
# this every commit lands on the openhands-agent account even when the push
# itself is authenticated as the repository owner.
set -uo pipefail

if [ -z "${GITHUB_TOKEN:-}" ]; then
  echo "GITHUB_TOKEN is not set; leaving the default commit identity in place."
  exit 0
fi

user_json="$(curl -fsSL -H "Authorization: Bearer ${GITHUB_TOKEN}" https://api.github.com/user 2>/dev/null || true)"
login="$(printf '%s' "$user_json" | python3 -c 'import sys,json; print(json.load(sys.stdin).get("login",""))' 2>/dev/null || true)"
uid="$(printf '%s' "$user_json" | python3 -c 'import sys,json; print(json.load(sys.stdin).get("id",""))' 2>/dev/null || true)"

if [ -n "$login" ] && [ -n "$uid" ]; then
  git config user.name "$login"
  git config user.email "${uid}+${login}@users.noreply.github.com"
  echo "Commits will be attributed to ${login} <${uid}+${login}@users.noreply.github.com>."
else
  echo "Could not resolve the token's GitHub identity; leaving the default commit identity in place."
fi
