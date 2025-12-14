# SofaScore Module - Known Issues and Fixes

## Status: Analysis Phase

This document tracks known issues in the SofaScoreLiveModule and their fixes.

## Branch
- **Branch**: `copilot/refactor-sofascore-module`
- **Created**: 2025-12-14
- **Purpose**: Fix errors in Sofascore module JSON parsing and data handling

## Known Issues

### Issue #1: NULL shortName Causes Crashes ❌ CRITICAL
**Status**: Identified  
**Severity**: HIGH - Causes crashes

**Problem**: 
- Some players have `null` or missing `shortName` in JSON
- Current code at line 689-698 in SofaScoreLiveModule.cpp doesn't handle this properly
- When `shortName` is null, `strlen(homeName)` on line 689 will crash

**Example Events with null shortName**:
- Event 15142775: Tata J. has `shortName: null`
- Event 15211627: Chrysospathis P. has `shortName: null`
- Multiple other events found

**Current Code**:
```cpp
const char* homeName = event["homeTeam"]["shortName"];
if (!homeName || strlen(homeName) == 0) {  // BAD: strlen on NULL crashes!
    homeName = event["homeTeam"]["name"];
}
```

**Fix Required**: Check for NULL before strlen()

---

### Issue #2: Empty Score Objects Not Handled ❌ CRITICAL
**Status**: Identified  
**Severity**: MEDIUM

**Problem**:
- Scheduled matches (status: "notstarted") have empty score objects `{}`
- Code accesses `homeScore["current"]` but object is empty
- This may return unexpected values or cause errors

**Example**:
```json
{
  "status": {"type": "notstarted"},
  "homeScore": {},
  "awayScore": {}
}
```

**Current Code**: Lines 700-721 assume score objects always have fields
**Fix Required**: Check if score objects are null/empty before accessing fields

---

### Issue #3: Date Filtering Logic is Incorrect ❌ CRITICAL  
**Status**: Identified  
**Severity**: HIGH - Shows "No matches today"

**Problem**:
The user reported that the date switch happens at 1 AM, not midnight. This suggests timezone handling issues with `GeneralTimeConverter`.

**Analysis from JSON files**:
- File `2025-12-13.json` contains matches from Dec 12 AND Dec 13
- File `2025-12-14.json` contains matches from Dec 13 AND Dec 14  
- File `2025-12-15.json` contains matches from Dec 14 AND Dec 15

**Current Logic** (lines 653-666):
```cpp
time_t now = time(nullptr);
time_t nowLocal = timeConverter.toLocal(now);

time_t matchTimestamp = event["startTimestamp"] | 0;
time_t matchLocal = timeConverter.toLocal(matchTimestamp);

if (!timeConverter.isSameDay(nowLocal, matchLocal)) {
    continue;  // Skip matches not happening today
}
```

**Issue**: `isSameDay()` compares using `localtime_r()` which uses SYSTEM timezone, NOT the GeneralTimeConverter's timezone!

**From GeneralTimeConverter.cpp line 83-94**:
```cpp
bool GeneralTimeConverter::isSameDay(time_t epoch1, time_t epoch2) const {
    time_t local1 = toLocal(epoch1);  // Converts to local using offset
    time_t local2 = toLocal(epoch2);

    struct tm tm1, tm2;
    localtime_r(&local1, &tm1);  // BUG: Uses SYSTEM TZ, not converter offset!
    localtime_r(&local2, &tm2);
    // ...
}
```

**Root Cause**: The `isSameDay()` function is broken! It converts to local time correctly with `toLocal()`, but then uses `localtime_r()` which applies the SYSTEM timezone AGAIN, causing double timezone offset.

**Fix Required**: Use `gmtime_r()` instead of `localtime_r()` in `isSameDay()`

---

### Issue #4: Page Numbering Starts at 0 ⚠️ MINOR
**Status**: Identified  
**Severity**: LOW - Confusing UX

**Problem**:
- Internal `_currentPage` starts at 0
- Display shows `_currentPage + 1` (lines 934, 1097)
- But resetPaging sets `_currentPage = 0` (line 325)

**Expected**: Pages should display as "1/5", not "0/5"
**Current**: Should be working correctly with `+ 1` in display

**Verification Needed**: Check if this is actually a problem or just needs verification

---

### Issue #5: Tournament ID Mismatch ⚠️ MEDIUM
**Status**: To Investigate  

**Problem**: 
JSON shows two different tournament IDs for "PDC World Championship":
- `tournament.id: 169922` (seasonal ID: "PDC World Championship 25/26")
- `tournament.id: 171078` (seasonal ID: "PDC World Championship 2026")

Code uses `event["tournament"]["id"]` (line 669) for filtering.

**Question**: Are users filtering by seasonal ID or unique tournament ID?
**Impact**: May filter out valid matches if IDs don't match configuration

---

### Issue #6: Missing NULL Checks Throughout ⚠️ MEDIUM
**Status**: Identified  
**Severity**: MEDIUM

**Problem**: Multiple places lack NULL/empty checks:
- Line 723: `tournamentName` - no NULL check before psram_strdup
- Line 726: `statusType` - no NULL check before strcmp  
- Lines 688-698: Player names - insufficient NULL handling

**Fix Required**: Add defensive NULL checks throughout parsing code

---

## Analysis Methodology

1. **Collect JSON samples** from SofaScore API
2. **Review parsing functions** in SofaScoreLiveModule.cpp:
   - `parseTournamentsJson()`
   - `parseDailyEventsJson()`
   - `parseMatchStatistics()`
3. **Identify mismatches** between expected and actual JSON structure
4. **Test edge cases** (missing fields, unexpected values, etc.)
5. **Fix parsing logic** to handle real-world data
6. **Verify fixes** with provided JSON samples

---

## Testing Plan

### Phase 1: JSON Structure Analysis
- [ ] Receive JSON files from user
- [ ] Parse and validate JSON structure
- [ ] Compare against current parsing code
- [ ] Document structural differences

### Phase 2: Issue Identification
- [ ] Identify parsing errors
- [ ] Identify missing null checks
- [ ] Identify incorrect field access
- [ ] Identify data type mismatches

### Phase 3: Code Fixes
- [ ] Fix parsing logic
- [ ] Add defensive programming
- [ ] Handle edge cases
- [ ] Add error logging

### Phase 4: Validation
- [ ] Test with all provided JSON samples
- [ ] Verify no crashes or errors
- [ ] Verify correct data extraction
- [ ] Document any API changes

---

## Previous Issues (Fixed)

### FreeRTOS Mutex Deadlock
- **Status**: ✅ Fixed in commit 7a3140a
- **Description**: Mutex deadlock in updateLiveMatchStats() causing assertion failures
- **Solution**: Refactored to separate data collection from network access

---

## Notes

- SofaScore API is unofficial and undocumented
- API structure may change without notice
- Defensive programming is essential
- Always check for null/undefined fields in JSON

---

Last updated: 2025-12-14
