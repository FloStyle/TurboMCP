"""Focused offline controls for the source-only daily publication decision."""
import copy
import unittest
from daily_release import daily_plan, REPOSITORY

A = "a" * 40
B = "b" * 40
RUN = {"name": "Repository validation", "path": ".github/workflows/validate.yml",
       "event": "push", "head_branch": "main", "head_repository": {"full_name": REPOSITORY},
       "status": "completed", "conclusion": "success", "head_sha": A}


class DailyReleaseTests(unittest.TestCase):
    def test_create_exact_daily_snapshot(self):
        plan = daily_plan(RUN, A, "2026-11-16", [])
        self.assertEqual((plan["action"], plan["tag"], plan["sha"]),
                         ("create", "daily-2026-11-16", A))

    def test_refuse_failed_fork_pr_other_branch_or_workflow(self):
        for key, value in [("conclusion", "failure"), ("conclusion", "cancelled"),
                           ("event", "pull_request"), ("head_branch", "stable"),
                           ("path", ".github/workflows/other.yml"),
                           ("head_repository", {"full_name": "other/fork"}),
                           ("head_sha", "main")]:
            with self.subTest(key=key, value=value):
                run = copy.deepcopy(RUN); run[key] = value
                with self.assertRaises(ValueError): daily_plan(run, A, "2026-11-16", [])

    def test_skip_stale_main(self):
        self.assertEqual(daily_plan(RUN, B, "2026-11-16", [])["action"], "skip")

    def test_unchanged_previous_day_does_not_publish(self):
        old = {"tag_name": "daily-2026-11-15", "target_commitish": A,
               "prerelease": True, "draft": False}
        self.assertEqual(daily_plan(RUN, A, "2026-11-16", [old])["action"], "skip")

    def test_update_only_same_day_release(self):
        releases = [{"id": 1, "tag_name": "v1.0.0", "target_commitish": B},
                    {"id": 2, "tag_name": "daily-2026-11-16", "target_commitish": B,
                     "prerelease": True, "draft": False}]
        plan = daily_plan(RUN, A, "2026-11-16", releases)
        self.assertEqual((plan["action"], plan["release_id"]), ("update", 2))

    def test_refuse_conflicting_daily_release_and_invalid_date(self):
        with self.assertRaises(ValueError):
            daily_plan(RUN, A, "2026-11-16", [{"tag_name": "daily-2026-11-16", "prerelease": False}])
        with self.assertRaises(ValueError): daily_plan(RUN, A, "2026-13-40", [])


if __name__ == "__main__":
    unittest.main()
