-- Persistent world-PvP vendetta ledger (Felworld): one row per
-- (victim bot, killer) pair, tallying the victim's unprovoked deaths to
-- that killer. Enough ganks (AiPlayerbot.WpvpVendettaGanks, or
-- WpvpVendettaCampGanks when any were camping re-kills) open a vendetta
-- that never expires; a revenge kill sets `settled`, and a fresh gank
-- clears it again. `last_gank_at` is epoch seconds.
DROP TABLE IF EXISTS `playerbots_wpvp_vendetta`;
CREATE TABLE `playerbots_wpvp_vendetta` (
    `victim_guid` INT UNSIGNED NOT NULL,
    `killer_guid` INT UNSIGNED NOT NULL,
    `ganks` INT UNSIGNED NOT NULL DEFAULT 0,
    `camps` INT UNSIGNED NOT NULL DEFAULT 0,
    `last_gank_at` INT UNSIGNED NOT NULL DEFAULT 0,
    `settled` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`victim_guid`, `killer_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
