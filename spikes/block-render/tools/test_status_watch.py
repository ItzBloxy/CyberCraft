import status_watch


def test_a_sample_has_the_status_age_the_chosen_lines_and_cets_frame_time(tmp_path):
    status = tmp_path / "status.txt"
    status.write_text("mode=gbuffer\nframe=12.000 ms average, 20.000 ms worst\nlink=linked; the mapping created\n"
                      "processes=Cyberpunk 85% of a core, 6100 MB; Minecraft not running\n", encoding="utf-8")
    (tmp_path / "frames.txt").write_text("CET's frame time: 12.1 ms average (82.6 fps), 19.0 ms worst\n", encoding="utf-8")
    sample = status_watch.sample(tmp_path, "frame|processes", status.stat().st_mtime + 2.5)
    assert sample == ["status written 2.5 s ago",
                      "frame=12.000 ms average, 20.000 ms worst",
                      "processes=Cyberpunk 85% of a core, 6100 MB; Minecraft not running",
                      "CET's frame time: 12.1 ms average (82.6 fps), 19.0 ms worst"]


def test_keys_match_whole_keys_only(tmp_path):
    status = tmp_path / "status.txt"
    status.write_text("link=linked\nlinked=no such key\nhud=always, scale 2\n", encoding="utf-8")
    assert status_watch.sample(tmp_path, "link|hud", status.stat().st_mtime) == [
        "status written 0.0 s ago", "link=linked", "hud=always, scale 2"]


def test_without_a_status_it_says_so(tmp_path):
    assert status_watch.sample(tmp_path, "frame", 0.0) == ["status.txt missing"]
