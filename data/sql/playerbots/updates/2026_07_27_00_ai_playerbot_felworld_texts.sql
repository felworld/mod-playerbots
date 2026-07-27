-- #########################################################
-- Playerbots - Texts for Felworld actions that previously
-- relied on hardcoded defaults (engineering combat items,
-- quest grinding, quest-competition groups).
-- IDs 30000+ are reserved for Felworld to avoid colliding
-- with upstream additions.
-- #########################################################

DELETE FROM `ai_playerbot_texts` WHERE `name` IN (
    'throw_explosive', 'sapper_charge', 'use_target_dummy', 'use_explosive_sheep',
    'use_jumper_cables', 'use_rocket_boots', 'grinding_quests', 'quest_competition_thanks');
DELETE FROM `ai_playerbot_texts_chance` WHERE `name` IN (
    'throw_explosive', 'sapper_charge', 'use_target_dummy', 'use_explosive_sheep',
    'use_jumper_cables', 'use_rocket_boots', 'grinding_quests', 'quest_competition_thanks');

INSERT INTO `ai_playerbot_texts` (`id`, `name`, `text`, `say_type`, `reply_type`,
    `text_loc1`, `text_loc2`, `text_loc3`, `text_loc4`,
    `text_loc5`, `text_loc6`, `text_loc7`, `text_loc8`) VALUES
    (30000, 'throw_explosive', 'Fire in the hole!', 0, 0, '', '', '', '', '', '', '', ''),
    (30001, 'throw_explosive', 'Bombs away!', 0, 0, '', '', '', '', '', '', '', ''),
    (30002, 'throw_explosive', 'Special delivery!', 0, 0, '', '', '', '', '', '', '', ''),
    (30003, 'throw_explosive', 'Catch!', 0, 0, '', '', '', '', '', '', '', ''),
    (30010, 'sapper_charge', 'Danger zone! Back up!', 0, 0, '', '', '', '', '', '', '', ''),
    (30011, 'sapper_charge', 'You are all standing in my blast radius!', 0, 0, '', '', '', '', '', '', '', ''),
    (30012, 'sapper_charge', 'Sapper charge — mind the crater!', 0, 0, '', '', '', '', '', '', '', ''),
    (30020, 'use_target_dummy', 'Go get the dummy!', 0, 0, '', '', '', '', '', '', '', ''),
    (30021, 'use_target_dummy', 'Deploying decoy!', 0, 0, '', '', '', '', '', '', '', ''),
    (30022, 'use_target_dummy', 'Talk to my little wooden friend!', 0, 0, '', '', '', '', '', '', '', ''),
    (30030, 'use_explosive_sheep', 'Sheep inbound!', 0, 0, '', '', '', '', '', '', '', ''),
    (30031, 'use_explosive_sheep', 'Baa means boom!', 0, 0, '', '', '', '', '', '', '', ''),
    (30032, 'use_explosive_sheep', 'Who let the sheep out?', 0, 0, '', '', '', '', '', '', '', ''),
    (30040, 'use_jumper_cables', 'Clear!', 0, 0, '', '', '', '', '', '', '', ''),
    (30041, 'use_jumper_cables', 'Hold still, this usually works!', 0, 0, '', '', '', '', '', '', '', ''),
    (30042, 'use_jumper_cables', 'Time for a jump-start!', 0, 0, '', '', '', '', '', '', '', ''),
    (30050, 'use_rocket_boots', 'Punching it!', 0, 0, '', '', '', '', '', '', '', ''),
    (30051, 'use_rocket_boots', 'Gotta go fast!', 0, 0, '', '', '', '', '', '', '', ''),
    (30052, 'use_rocket_boots', 'Rocket boots, engage!', 0, 0, '', '', '', '', '', '', '', ''),
    (30060, 'grinding_quests', 'Grinding quest mobs', 0, 0, '', '', '', '', '', '', '', ''),
    (30061, 'grinding_quests', 'On it — thinning out the quest mobs', 0, 0, '', '', '', '', '', '', '', ''),
    (30070, 'quest_competition_thanks', 'Thanks for the group, that''s everything I needed!', 0, 0, '', '', '', '', '', '', '', ''),
    (30071, 'quest_competition_thanks', 'That''s all I needed — thanks for the help!', 0, 0, '', '', '', '', '', '', '', ''),
    (30072, 'quest_competition_thanks', 'All done here, thanks for sharing the spawns!', 0, 0, '', '', '', '', '', '', '', '');

INSERT INTO `ai_playerbot_texts_chance` (`name`, `probability`) VALUES
    ('throw_explosive', 100),
    ('sapper_charge', 100),
    ('use_target_dummy', 100),
    ('use_explosive_sheep', 100),
    ('use_jumper_cables', 100),
    ('use_rocket_boots', 100),
    ('grinding_quests', 100),
    ('quest_competition_thanks', 100);
