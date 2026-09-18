# Update Checking

## Current behavior

The application attempts to check the latest GitHub stable Release two seconds after startup. Automatic checks honor the configured enable switch and interval, which defaults to 24 hours. The “Check for updates” action in Settings always performs a manual check and bypasses the interval. All requests use the project `NetworkClient` and `RequestProfiles::updateCheck()`.

```mermaid
stateDiagram-v2
    [*] --> NotChecked
    NotChecked --> Checking: Startup or manual check
    Checking --> UpdateAvailable: Stable Release is newer
    Checking --> UpToDate: Version is not newer
    Checking --> Failed: Network, rate limit, or validation failure
    Checking --> NotChecked: Request cancelled
    Failed --> Checking: Retry
    UpdateAvailable --> Ignored: Ignore or remind later
    Ignored --> UpdateAvailable: A newer version is found
```

## Local configuration and cache

Update checking reuses `ConfigService` and does not introduce a separate settings store. The persisted fields are:

- `updateAutoCheck`: whether startup checks are enabled; enabled by default.
- `updateCheckIntervalHours`: automatic check interval, 24 hours by default and limited to 1–168 hours.
- `updateLastCheckTime`: the most recent request start time.
- `updateLastSuccessfulCheckTime`: the most recent successfully validated Release time.
- `updateCachedRelease`: the most recent valid Release JSON.
- `updateIgnoredVersion`: the version explicitly ignored by the user.
- `updateRemindedVersion`: the version postponed by the user.

When the network fails, a valid local Release cache is applied first and the state is then reported as failed. This keeps cached update information available while the Settings page still reports the failed check. Cache and ignore records are cleared when a newer version is found.

“Remind later” hides the current version and remains effective after restart. “Ignore this version” expresses a stronger preference and suppresses the current version until a newer version appears.

## Version and Release rules

Version comparison follows the basic SemVer rules and accepts `v3.1.13`, `3.1.13-beta.1`, `3.1.13-rc.1`, and versions with build metadata. Major, minor, and patch components are compared in order. A prerelease is lower than the stable version with the same base version; prerelease identifiers follow numeric and lexical ordering. Build metadata does not affect precedence. Invalid versions fail safely and never report a false update.

Only published stable Releases are accepted by default. Drafts, prereleases, and responses missing `tag_name`, `name`, a valid HTTPS `html_url`, or `assets` enter the failed state. The update link first selects an asset matching the current platform and architecture; if no reliable match exists, it falls back to the Release page.

## User interface

The update Banner shows checking, update available, and failed states. When an update is available, users can open the matching platform asset or Release page, postpone the notification, or ignore the version. Failed checks provide a retry action. The Settings sidebar contains the current version, check status, startup auto-check switch, and manual check button.

## Limitations

GitHub ETag support is not implemented yet, and the application does not download or install updates automatically. The selected link is opened in the system browser; remote files are never executed directly.
