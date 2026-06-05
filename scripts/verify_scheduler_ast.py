#!/usr/bin/env python3
"""Verify scheduler AST output from a Hojicha serial log."""

from __future__ import annotations

import argparse
import re
import sys
import unittest
from dataclasses import dataclass
from pathlib import Path


EVENT_PREFIX = "AST_SCHEDULER:"
DEFAULT_LOG_PATH = Path("logs/serial.log")
ANSI_RE = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")
CONTROL_RE = re.compile(r"[\x00-\x08\x0b\x0c\x0e-\x1f\x7f]")
MONITOR_TICK_RE = re.compile(r"^monitor tick ([0-9]+)$")


@dataclass(frozen=True)
class Event:
    line_no: int
    text: str


@dataclass
class LogAnalysis:
    events: list[Event]
    warnings: list[str]

    @property
    def max_tick(self) -> int:
        ticks = self.monitor_ticks()
        if not ticks:
            return 0
        return max(tick for _, tick in ticks)

    def monitor_ticks(self) -> list[tuple[Event, int]]:
        parsed = []
        for event in self.events:
            match = MONITOR_TICK_RE.match(event.text)
            if match is not None:
                parsed.append((event, int(match.group(1))))
        return parsed

    def first(self, text: str) -> Event | None:
        for event in self.events:
            if event.text == text:
                return event
        return None

    def all(self, text: str) -> list[Event]:
        return [event for event in self.events if event.text == text]

    def require(
        self,
        test_case: unittest.TestCase,
        text: str,
        min_tick: int | None = None,
    ) -> Event | None:
        event = self.first(text)
        if event is not None:
            return event

        if min_tick is not None and self.max_tick < min_tick:
            self.warnings.append(
                f"partial run: missing '{text}' because the log only reached "
                f"monitor tick {self.max_tick}; expected at tick {min_tick}"
            )
            return None

        test_case.fail(f"missing scheduler AST event: '{text}'")


def clean_line(line: str) -> str:
    line = ANSI_RE.sub("", line)
    line = CONTROL_RE.sub("", line)
    return line.strip()


def analyze_log(path: Path) -> LogAnalysis:
    events: list[Event] = []
    with path.open("r", encoding="utf-8", errors="replace") as log_file:
        for line_no, raw_line in enumerate(log_file, 1):
            line = clean_line(raw_line)
            marker = line.find(EVENT_PREFIX)
            if marker == -1:
                continue
            text = line[marker + len(EVENT_PREFIX):].strip()
            events.append(Event(line_no=line_no, text=text))
    return LogAnalysis(events=events, warnings=[])


class SchedulerAstAssertions(unittest.TestCase):
    analysis: LogAnalysis

    @classmethod
    def set_analysis(cls, analysis: LogAnalysis) -> None:
        cls.analysis = analysis

    def require(self, text: str, min_tick: int | None = None) -> Event | None:
        return self.analysis.require(self, text, min_tick)

    def assert_before(
        self,
        earlier: Event | None,
        later: Event | None,
        message: str,
    ) -> None:
        if earlier is None or later is None:
            return
        self.assertLess(earlier.line_no, later.line_no, message)

    def test_startup_markers(self) -> None:
        starting = self.require("starting")
        added = self.require("processes added")
        self.assert_before(
            starting,
            added,
            "'starting' should appear before 'processes added'",
        )

    def test_core_runtime_events(self) -> None:
        self.require("waker awake", min_tick=5)
        self.require("watch_1 observed wake", min_tick=5)
        self.require("watch_2 observed wake", min_tick=5)
        self.require("sleep_once_1 complete", min_tick=7)
        self.require("sleep_once_2 complete", min_tick=7)

    def test_semaphore_ordering(self) -> None:
        owner_requested = self.require("sem_owner lock requested")
        owner_acquired = self.require("sem_owner lock acquired")
        owner_unlock = self.require("sem_owner unlock", min_tick=7)

        waiter_requested = self.require("sem_waiter lock requested")
        waiter_acquired = self.require("sem_waiter lock acquired", min_tick=7)
        waiter_unlock = self.require("sem_waiter unlock", min_tick=7)

        self.assert_before(
            owner_requested,
            owner_acquired,
            "semaphore owner acquired before requesting",
        )
        self.assert_before(
            owner_acquired,
            owner_unlock,
            "semaphore owner unlocked before acquiring",
        )
        self.assert_before(
            waiter_requested,
            waiter_acquired,
            "semaphore waiter acquired before requesting",
        )
        self.assert_before(
            waiter_acquired,
            waiter_unlock,
            "semaphore waiter unlocked before acquiring",
        )
        self.assert_before(
            owner_unlock,
            waiter_acquired,
            "semaphore waiter acquired before owner unlocked",
        )

    def test_try_lock_outcomes(self) -> None:
        self.require("try_fail lock requested")
        self.require("try_fail lock denied")
        self.require("try_success lock requested", min_tick=20)
        self.require("try_success lock acquired", min_tick=20)

        self.assertEqual(
            [],
            self.analysis.all("try_fail unexpectedly acquired"),
            "try_fail acquired the semaphore while it should have been held",
        )
        self.assertEqual(
            [],
            self.analysis.all("try_success lock denied"),
            "try_success failed after the semaphore should have been released",
        )

    def test_monitor_ticks_increase(self) -> None:
        ticks = self.analysis.monitor_ticks()
        self.assertNotEqual([], ticks, "missing monitor ticks")
        previous = 0
        for event, tick in ticks:
            self.assertGreater(
                tick,
                previous,
                f"monitor tick at line {event.line_no} did not increase: {tick} after {previous}",
            )
            previous = tick

    def test_watch_termination(self) -> None:
        watch_2_term = self.require("terminating watch_2", min_tick=15)
        watch_1_term = self.require("terminating watch_1", min_tick=21)

        tick_15 = self.analysis.first("monitor tick 15")
        tick_21 = self.analysis.first("monitor tick 21")
        if watch_2_term is not None:
            self.assertIsNotNone(
                tick_15,
                "watch_2 terminated before monitor tick 15 was logged",
            )
            self.assert_before(
                tick_15,
                watch_2_term,
                "watch_2 termination should follow monitor tick 15",
            )
            later_wakes = [
                event
                for event in self.analysis.all("watch_2 observed wake")
                if event.line_no > watch_2_term.line_no
            ]
            self.assertEqual([], later_wakes, "watch_2 woke after termination")

        if watch_1_term is not None:
            self.assertIsNotNone(
                tick_21,
                "watch_1 terminated before monitor tick 21 was logged",
            )
            self.assert_before(
                tick_21,
                watch_1_term,
                "watch_1 termination should follow monitor tick 21",
            )
            later_wakes = [
                event
                for event in self.analysis.all("watch_1 observed wake")
                if event.line_no > watch_1_term.line_no
            ]
            self.assertEqual([], later_wakes, "watch_1 woke after termination")


class AssertionResult(unittest.TextTestResult):
    def _exc_info_to_string(self, err, test):
        if issubclass(err[0], AssertionError):
            message = str(err[1])
            if message:
                return f"assertion failed: {message}"
            return "assertion failed"
        return super()._exc_info_to_string(err, test)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Verify Hojicha scheduler AST output in a serial log."
    )
    parser.add_argument(
        "log_path",
        nargs="?",
        default=DEFAULT_LOG_PATH,
        type=Path,
        help=f"serial log path (default: {DEFAULT_LOG_PATH})",
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if not args.log_path.exists():
        print(f"error: log file does not exist: {args.log_path}", file=sys.stderr)
        return 2

    analysis = analyze_log(args.log_path)
    if not analysis.events:
        print(
            f"error: no {EVENT_PREFIX} events found in {args.log_path}",
            file=sys.stderr,
        )
        return 2

    SchedulerAstAssertions.set_analysis(analysis)
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(SchedulerAstAssertions)
    runner = unittest.TextTestRunner(
        resultclass=AssertionResult,
        verbosity=2,
    )
    result = runner.run(suite)

    for warning in sorted(set(analysis.warnings)):
        print(f"warning: {warning}", file=sys.stderr)

    print(
        f"checked {len(analysis.events)} scheduler AST events; "
        f"max monitor tick: {analysis.max_tick}",
        file=sys.stderr,
    )
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
