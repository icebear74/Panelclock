#!/usr/bin/env python3
"""
Test the SofaScore JSON parsing logic with actual JSON files.
This simulates what the C++ code does and validates the fixes.
"""

import json
import sys
from datetime import datetime, timezone, timedelta

def parse_and_filter_events(json_file, today_date_str, tz_offset_hours=1):
    """
    Simulate the C++ parsing logic with timezone handling.
    
    Args:
        json_file: Path to JSON file
        today_date_str: Date to consider as "today" (YYYY-MM-DD)
        tz_offset_hours: Timezone offset in hours (e.g., 1 for CET/UTC+1)
    """
    print(f"\n{'='*80}")
    print(f"Testing with file: {json_file}")
    print(f"Considering 'today' as: {today_date_str}")
    print(f"Timezone offset: UTC+{tz_offset_hours}")
    print(f"{'='*80}\n")
    
    with open(json_file, 'r') as f:
        data = json.load(f)
    
    events = data.get('events', [])
    print(f"Total events in file: {len(events)}")
    
    # Parse today's date
    today = datetime.strptime(today_date_str, '%Y-%m-%d')
    today_local = today.replace(tzinfo=timezone(timedelta(hours=tz_offset_hours)))
    
    filtered_events = []
    issues = []
    
    for i, event in enumerate(events):
        event_id = event.get('id', 0)
        
        # Check for NULL shortName issue
        home_short = event.get('homeTeam', {}).get('shortName')
        away_short = event.get('awayTeam', {}).get('shortName')
        
        if home_short is None:
            issues.append(f"Event {event_id}: homeTeam shortName is NULL")
        if away_short is None:
            issues.append(f"Event {event_id}: awayTeam shortName is NULL")
        
        # Get player names (with fallback logic)
        home_name = home_short if home_short else event.get('homeTeam', {}).get('name', 'Unknown')
        away_name = away_short if away_short else event.get('awayTeam', {}).get('name', 'Unknown')
        
        # Check for empty score objects
        home_score = event.get('homeScore', {})
        away_score = event.get('awayScore', {})
        
        if not home_score or len(home_score) == 0:
            issues.append(f"Event {event_id}: homeScore is empty")
        if not away_score or len(away_score) == 0:
            issues.append(f"Event {event_id}: awayScore is empty")
        
        # Parse timestamp and convert to local timezone
        timestamp = event.get('startTimestamp', 0)
        if timestamp:
            event_utc = datetime.fromtimestamp(timestamp, tz=timezone.utc)
            event_local = event_utc.astimezone(timezone(timedelta(hours=tz_offset_hours)))
            
            # Check if same day (FIXED logic - just compare date components)
            is_same_day = (event_local.date() == today_local.date())
            
            if is_same_day:
                filtered_events.append({
                    'id': event_id,
                    'time_utc': event_utc.strftime('%Y-%m-%d %H:%M:%S UTC'),
                    'time_local': event_local.strftime('%Y-%m-%d %H:%M:%S'),
                    'home': home_name,
                    'away': away_name,
                    'status': event.get('status', {}).get('type', 'unknown'),
                    'tournament': event.get('tournament', {}).get('name', 'Unknown'),
                    'tournament_id': event.get('tournament', {}).get('id', 0)
                })
    
    print(f"\nMatches on {today_date_str} (after filtering): {len(filtered_events)}")
    print(f"\n{'Match Time':<25} {'Home':<20} {'Away':<20} {'Status':<12} {'Tournament'}")
    print("-" * 100)
    
    for event in filtered_events[:10]:  # Show first 10
        print(f"{event['time_local']:<25} {event['home']:<20} {event['away']:<20} {event['status']:<12} {event['tournament']}")
    
    if len(filtered_events) > 10:
        print(f"... and {len(filtered_events) - 10} more")
    
    if issues:
        print(f"\n⚠️  Issues found: {len(issues)}")
        for issue in issues[:10]:  # Show first 10 issues
            print(f"  - {issue}")
        if len(issues) > 10:
            print(f"  ... and {len(issues) - 10} more")
    
    return filtered_events, issues


def main():
    # Test scenarios based on the JSON files
    test_cases = [
        ('json/2025-12-13.json', '2025-12-13'),  # Should show matches from Dec 13
        ('json/2025-12-14.json', '2025-12-14'),  # Should show matches from Dec 14
        ('json/2025-12-15.json', '2025-12-15'),  # Should show matches from Dec 15
        
        # Edge case: What if we query yesterday's file for today?
        ('json/2025-12-13.json', '2025-12-12'),  # Should show matches from Dec 12
    ]
    
    all_issues = []
    
    for json_file, today_str in test_cases:
        try:
            events, issues = parse_and_filter_events(json_file, today_str, tz_offset_hours=1)
            all_issues.extend(issues)
        except Exception as e:
            print(f"ERROR processing {json_file}: {e}")
    
    print(f"\n{'='*80}")
    print("SUMMARY OF ALL ISSUES")
    print(f"{'='*80}")
    
    if all_issues:
        # Count unique issues
        issue_types = {}
        for issue in all_issues:
            if 'shortName is NULL' in issue:
                issue_types['NULL shortName'] = issue_types.get('NULL shortName', 0) + 1
            elif 'Score is empty' in issue:
                issue_types['Empty score object'] = issue_types.get('Empty score object', 0) + 1
        
        for issue_type, count in issue_types.items():
            print(f"  {issue_type}: {count} occurrences")
    else:
        print("✅ No issues found!")
    
    print(f"\n{'='*80}")
    print("VALIDATION COMPLETE")
    print(f"{'='*80}")


if __name__ == '__main__':
    main()
