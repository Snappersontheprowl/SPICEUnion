import spiceunion


def main() -> None:
    assert isinstance(spiceunion.version(), str)
    assert spiceunion.version()
    assert isinstance(spiceunion.libpsf_reader_enabled(), bool)
    assert spiceunion.ResultStatus.OK.name == "OK"

    failed = spiceunion.read_dc_value("missing.raw", "vout")
    assert not failed.ok()
    assert failed.status in {
        spiceunion.ResultStatus.FILE_NOT_FOUND,
        spiceunion.ResultStatus.UNSUPPORTED_FORMAT,
    }


if __name__ == "__main__":
    main()
