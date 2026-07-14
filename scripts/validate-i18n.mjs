#!/usr/bin/env node

import { createHash } from "node:crypto";
import { access, readFile, writeFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const manifestPath = path.join(root, "docs", "i18n", "manifest.json");
const updateHash = process.argv.includes("--update-hash");
const manifest = JSON.parse(await readFile(manifestPath, "utf8"));
const errors = [];

const toPosix = (value) => value.split(path.sep).join("/");
const absolute = (relativePath) => path.resolve(root, relativePath);
const exists = async (target) => {
  try {
    await access(target);
    return true;
  } catch {
    return false;
  }
};

if (manifest.locales.length !== 10) {
  errors.push(`Expected 10 locales, found ${manifest.locales.length}.`);
}

const canonicalPath = absolute(manifest.canonical);
const canonical = await readFile(canonicalPath, "utf8");
const canonicalHash = createHash("sha256").update(canonical).digest("hex");
const codeBlocks = [...canonical.matchAll(/```[^\n]*\n[\s\S]*?```/g)].map((match) => match[0]);

if (!updateHash && canonicalHash !== manifest.canonicalSha256) {
  errors.push(
    `Canonical README changed (${canonicalHash}); translations may be stale. Review every locale, then run this script with --update-hash.`,
  );
}

for (const locale of manifest.locales) {
  const localePath = absolute(locale.path);
  if (!(await exists(localePath))) {
    errors.push(`Missing ${locale.code}: ${locale.path}`);
    continue;
  }

  const markdown = await readFile(localePath, "utf8");
  const localeBlocks = [...markdown.matchAll(/```[^\n]*\n[\s\S]*?```/g)].map((match) => match[0]);
  if (localeBlocks.length !== codeBlocks.length || localeBlocks.some((block, index) => block !== codeBlocks[index])) {
    errors.push(`${locale.path}: fenced code blocks differ from the canonical README.`);
  }

  const htmlLinks = [...markdown.matchAll(/href="([^"]+)"/g)].map((match) => match[1]);
  for (const targetLocale of manifest.locales) {
    const targetPath = absolute(targetLocale.path);
    const selectorHasTarget = htmlLinks.some((href) => {
      if (/^[a-z][a-z0-9+.-]*:/i.test(href) || href.startsWith("#")) return false;
      return path.resolve(path.dirname(localePath), href.split(/[?#]/, 1)[0]) === targetPath;
    });
    if (!selectorHasTarget) {
      errors.push(`${locale.path}: language selector does not link to ${targetLocale.code}.`);
    }
  }

  const localTargets = [
    ...markdown.matchAll(/(?:href|src)="([^"]+)"/g),
    ...markdown.matchAll(/!?\[[^\]]*\]\(([^)]+)\)/g),
  ].map((match) => match[1]);

  for (const rawTarget of localTargets) {
    const target = rawTarget.trim().replace(/^<|>$/g, "").split(/[?#]/, 1)[0];
    if (!target || target.startsWith("#") || /^[a-z][a-z0-9+.-]*:/i.test(target)) continue;
    const resolved = path.resolve(path.dirname(localePath), target);
    if (!(await exists(resolved))) {
      errors.push(`${locale.path}: missing local target ${target} (${toPosix(path.relative(root, resolved))}).`);
    }
  }
}

if (errors.length > 0) {
  for (const error of errors) console.error(`ERROR: ${error}`);
  process.exitCode = 1;
} else if (updateHash) {
  manifest.canonicalSha256 = canonicalHash;
  await writeFile(manifestPath, `${JSON.stringify(manifest, null, 2)}\n`, "utf8");
  console.log(`Updated canonical SHA-256: ${canonicalHash}`);
} else {
  console.log(`i18n validation passed for ${manifest.locales.length} locales (${canonicalHash}).`);
}
