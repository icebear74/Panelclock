#!/usr/bin/env python3
"""
SofaScore JSON Analyzer

This script analyzes JSON files from the SofaScore API to help identify
parsing issues in the SofaScoreLiveModule.

Usage:
    python analyze_json.py <json_file>
    python analyze_json.py test_data/sofascore/*.json
"""

import json
import sys
import os
from typing import Dict, Any, List
from pathlib import Path


def analyze_tournament_json(data: Dict[str, Any]) -> None:
    """Analyze tournament list JSON structure."""
    print("\n=== TOURNAMENT LIST ANALYSIS ===")
    
    if 'groups' in data:
        print(f"Found {len(data['groups'])} groups")
        for i, group in enumerate(data['groups'][:3]):  # Show first 3
            print(f"\nGroup {i}:")
            if 'uniqueTournaments' in group:
                print(f"  Tournaments: {len(group['uniqueTournaments'])}")
                for j, tournament in enumerate(group['uniqueTournaments'][:2]):
                    print(f"    Tournament {j}:")
                    print(f"      ID: {tournament.get('id', 'N/A')}")
                    print(f"      Name: {tournament.get('name', 'N/A')}")
                    print(f"      Slug: {tournament.get('slug', 'N/A')}")
                    print(f"      Category: {tournament.get('category', {}).get('name', 'N/A')}")
    else:
        print("WARNING: No 'groups' field found!")


def analyze_daily_events_json(data: Dict[str, Any]) -> None:
    """Analyze daily events JSON structure."""
    print("\n=== DAILY EVENTS ANALYSIS ===")
    
    if 'events' in data:
        print(f"Found {len(data['events'])} events")
        for i, event in enumerate(data['events'][:3]):  # Show first 3
            print(f"\nEvent {i}:")
            print(f"  ID: {event.get('id', 'N/A')}")
            print(f"  Status: {event.get('status', {}).get('type', 'N/A')}")
            print(f"  Home: {event.get('homeTeam', {}).get('name', 'N/A')}")
            print(f"  Away: {event.get('awayTeam', {}).get('name', 'N/A')}")
            
            if 'homeScore' in event:
                print(f"  Home Score: {event['homeScore'].get('current', 0)}")
            if 'awayScore' in event:
                print(f"  Away Score: {event['awayScore'].get('current', 0)}")
                
            if 'tournament' in event:
                print(f"  Tournament: {event['tournament'].get('name', 'N/A')}")
                print(f"  Tournament ID: {event['tournament'].get('id', 'N/A')}")
                
            print(f"  Start Timestamp: {event.get('startTimestamp', 'N/A')}")
    else:
        print("WARNING: No 'events' field found!")


def analyze_statistics_json(data: Dict[str, Any]) -> None:
    """Analyze match statistics JSON structure."""
    print("\n=== MATCH STATISTICS ANALYSIS ===")
    
    if 'statistics' in data:
        print(f"Found {len(data['statistics'])} statistic periods")
        for i, period in enumerate(data['statistics']):
            print(f"\nPeriod {i} ({period.get('period', 'N/A')}):")
            if 'groups' in period:
                for j, group in enumerate(period['groups']):
                    print(f"  Group {j} ({group.get('groupName', 'N/A')}):")
                    if 'statisticsItems' in group:
                        print(f"    Items: {len(group['statisticsItems'])}")
                        for item in group['statisticsItems'][:5]:  # Show first 5
                            name = item.get('name', 'N/A')
                            home = item.get('home', 'N/A')
                            away = item.get('away', 'N/A')
                            print(f"      {name}: {home} | {away}")
    else:
        print("WARNING: No 'statistics' field found!")


def analyze_event_json(data: Dict[str, Any]) -> None:
    """Analyze single event/match JSON structure."""
    print("\n=== EVENT DETAILS ANALYSIS ===")
    
    if 'event' in data:
        event = data['event']
        print(f"Event ID: {event.get('id', 'N/A')}")
        print(f"Status: {event.get('status', {}).get('type', 'N/A')}")
        print(f"Status Description: {event.get('status', {}).get('description', 'N/A')}")
        print(f"Home Team: {event.get('homeTeam', {}).get('name', 'N/A')}")
        print(f"Away Team: {event.get('awayTeam', {}).get('name', 'N/A')}")
        
        if 'homeScore' in event:
            print(f"Home Score - Current: {event['homeScore'].get('current', 'N/A')}")
            print(f"Home Score - Period1: {event['homeScore'].get('period1', 'N/A')}")
        
        if 'awayScore' in event:
            print(f"Away Score - Current: {event['awayScore'].get('current', 'N/A')}")
            print(f"Away Score - Period1: {event['awayScore'].get('period1', 'N/A')}")
            
        if 'tournament' in event:
            print(f"Tournament: {event['tournament'].get('name', 'N/A')}")
            print(f"Tournament ID: {event['tournament'].get('id', 'N/A')}")
    else:
        print("WARNING: No 'event' field found in root!")


def detect_json_type(data: Dict[str, Any]) -> str:
    """Detect what type of SofaScore JSON this is."""
    if 'groups' in data and isinstance(data.get('groups', []), list):
        first_group = data['groups'][0] if data['groups'] else {}
        if 'uniqueTournaments' in first_group:
            return 'tournaments'
    
    if 'events' in data and isinstance(data.get('events', []), list):
        return 'daily_events'
    
    if 'statistics' in data and isinstance(data.get('statistics', []), list):
        return 'statistics'
    
    if 'event' in data and isinstance(data.get('event', {}), dict):
        return 'event'
    
    return 'unknown'


def analyze_json_file(filepath: str) -> None:
    """Analyze a single JSON file."""
    print(f"\n{'='*60}")
    print(f"Analyzing: {filepath}")
    print(f"{'='*60}")
    
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        json_type = detect_json_type(data)
        print(f"Detected Type: {json_type.upper()}")
        
        if json_type == 'tournaments':
            analyze_tournament_json(data)
        elif json_type == 'daily_events':
            analyze_daily_events_json(data)
        elif json_type == 'statistics':
            analyze_statistics_json(data)
        elif json_type == 'event':
            analyze_event_json(data)
        else:
            print("WARNING: Could not detect JSON type!")
            print("Root keys:", list(data.keys()))
        
        # Always show structure
        print("\n=== JSON STRUCTURE ===")
        print_structure(data, max_depth=3)
        
    except json.JSONDecodeError as e:
        print(f"ERROR: Invalid JSON - {e}")
    except Exception as e:
        print(f"ERROR: {e}")


def print_structure(obj: Any, indent: int = 0, max_depth: int = 3, current_depth: int = 0) -> None:
    """Print the structure of a JSON object."""
    if current_depth >= max_depth:
        return
    
    prefix = "  " * indent
    
    if isinstance(obj, dict):
        for key, value in list(obj.items())[:10]:  # Limit to first 10 keys
            if isinstance(value, dict):
                print(f"{prefix}{key}: {{...}} ({len(value)} keys)")
                print_structure(value, indent + 1, max_depth, current_depth + 1)
            elif isinstance(value, list):
                print(f"{prefix}{key}: [...] ({len(value)} items)")
                if value and current_depth < max_depth - 1:
                    print(f"{prefix}  First item:")
                    print_structure(value[0], indent + 2, max_depth, current_depth + 1)
            else:
                print(f"{prefix}{key}: {type(value).__name__}")
    elif isinstance(obj, list):
        if obj:
            print(f"{prefix}List with {len(obj)} items, first item:")
            print_structure(obj[0], indent, max_depth, current_depth + 1)


def main():
    """Main entry point."""
    if len(sys.argv) < 2:
        print("Usage: python analyze_json.py <json_file> [json_file2 ...]")
        print("Example: python analyze_json.py test_data/sofascore/*.json")
        sys.exit(1)
    
    files = []
    for arg in sys.argv[1:]:
        if '*' in arg or '?' in arg:
            # Handle glob patterns
            from glob import glob
            files.extend(glob(arg))
        else:
            files.append(arg)
    
    if not files:
        print("No files found!")
        sys.exit(1)
    
    for filepath in files:
        if os.path.exists(filepath):
            analyze_json_file(filepath)
        else:
            print(f"ERROR: File not found: {filepath}")
    
    print(f"\n{'='*60}")
    print(f"Analysis complete! Processed {len(files)} file(s).")
    print(f"{'='*60}")


if __name__ == '__main__':
    main()
