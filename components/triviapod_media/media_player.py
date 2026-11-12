import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import media_player, speaker
from esphome.components.speaker.media_player import SpeakerMediaPlayer
from esphome.const import CONF_ID

DEPENDENCIES = ["network"]
ns = cg.esphome_ns.namespace("triviapod_media")
GuardedMediaPlayer = ns.class_("GuardedMediaPlayer", cg.Component, media_player.MediaPlayer)
CONFIG_SCHEMA = media_player.media_player_schema(GuardedMediaPlayer).extend({
    cv.Required("backend"): cv.use_id(SpeakerMediaPlayer),
    cv.Required("speaker_id"): cv.use_id(speaker.Speaker),
    cv.Required("local_busy"): cv.returning_lambda,
    cv.Required("amplifier"): cv.returning_lambda,
    cv.Optional("on_busy"): automation.validate_automation(single=True),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = await media_player.new_media_player(config)
    await cg.register_component(var, config)
    cg.add(var.set_backend(await cg.get_variable(config["backend"])))
    cg.add(var.set_speaker(await cg.get_variable(config["speaker_id"])))
    cg.add(var.set_local_busy(await cg.process_lambda(config["local_busy"], [], return_type=cg.bool_)))
    cg.add(var.set_amplifier(await cg.process_lambda(config["amplifier"], [(cg.bool_, "x")], return_type=cg.bool_)))
    if "on_busy" in config:
        await automation.build_automation(var.get_busy_trigger(), [(cg.bool_, "x")], config["on_busy"])
