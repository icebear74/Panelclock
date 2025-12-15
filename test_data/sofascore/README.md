# SofaScore Module Test Data

This directory contains JSON test files from the SofaScore API for analyzing and debugging the SofaScoreLiveModule.

## Purpose

The SofaScore module needs refactoring to fix errors in JSON parsing and data processing. This directory will store real API responses for testing.

## Expected Files

Place JSON files from the following SofaScore API endpoints here:

### 1. Tournament List
- **Endpoint**: `/config/unique-tournaments/de/darts`
- **File name**: `tournaments.json`
- **Purpose**: Test tournament parsing

### 2. Daily Events
- **Endpoint**: `/sport/darts/scheduled-events/YYYY-MM-DD`
- **File name**: `daily_events_YYYY-MM-DD.json`
- **Purpose**: Test daily match parsing

### 3. Match Statistics
- **Endpoint**: `/event/{eventId}/statistics`
- **File name**: `statistics_{eventId}.json`
- **Purpose**: Test live statistics parsing

### 4. Match Details
- **Endpoint**: `/event/{eventId}`
- **File name**: `event_{eventId}.json`
- **Purpose**: Test match detail parsing

## How to Use

1. Place JSON files in this directory
2. Run analysis script to identify parsing issues
3. Use files for unit testing the fixes
4. Document any issues found in `SOFASCORE_ISSUES.md`

## Notes

- JSON files should be prettified for easier reading
- Include both successful and edge-case responses
- Document the date/time when each JSON was captured
