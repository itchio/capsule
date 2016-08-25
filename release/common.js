// base functions useful throughout CI scripts

const $ = require('munyx')
const child_process = require('child_process')

$.app_name = function () {
  return 'capsule'
}

const CMD_PATH = 'cmd.exe'

function cmd_system (args, opts = {}) {
  const res = child_process.spawnSync(CMD_PATH, ['/S', '/C', ...args], {
    stdio: 'inherit'
  })

  if (res.error) {
    $.putln(`☃ ${res.error.toString()}`.red)
    return false
  }
  if (res.status !== 0) {
    $.putln(`☃ non-zero exit code: ${res.status}`.red)
    return false
  }
  return true
}

$.cmd = function (args) {
  $.putln(`· ${args.join(' ')}`.blue)
  return cmd_system(args)
}

module.exports = $
