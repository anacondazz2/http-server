# Run as `sudo ./tunnel-nohup.sh`
# sudo will have different PATH and ~ expansion, so provide full paths below...

CLOUDFLARED="/home/pex/Applications/cloudflared/cloudflared"
CONFIG="/home/pex/.cloudflared/config.yml"
CMD="$CLOUDFLARED tunnel --config $CONFIG run"
LOG="/var/log/cloudflared.log"
nohup $CMD > "$LOG" 2>&1 &
