#!/usr/bin/env python3
import argparse

def main(date):
    print("Generating static HTML for", date)

if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--date", required=True)
    args = p.parse_args()
    main(args.date)
