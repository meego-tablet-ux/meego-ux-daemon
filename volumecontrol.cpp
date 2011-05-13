/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include "volumecontrol.h"

#include <stdio.h>
#include <functional>

pa_context * m_context = NULL;
pa_glib_mainloop* m_mainloop = NULL;

static VolumeControlPrivate* m_instance = NULL;

void subscribedEventsCallback(pa_context *c, enum pa_subscription_event_type t, uint32_t index, void *);
void pulseSinkInfoCallback(pa_context *c, const pa_sink_info *info, int eol, void *userdata);
void contextStateCallback(pa_context* c, void* );

VolumeControl::VolumeControl(QObject *parent)
        :QObject(parent),volumeControl(NULL)
{
        volumeControl = VolumeControlPrivate::instance();

        connect(volumeControl,SIGNAL(muteChanged(bool)),this, SIGNAL(muteChanged(bool)));
        connect(volumeControl,SIGNAL(volumeChanged(int)),this, SIGNAL(volumeChanged(int)));

        volumeChanged(volumeControl->volume());
}

VolumeControlPrivate * VolumeControlPrivate::instance()
{
        if(!m_instance) new VolumeControlPrivate();
        return m_instance;
}

VolumeControlPrivate::VolumeControlPrivate(QObject* parent)
:QObject(parent),m_sink(NULL)
{
        m_instance = this;

        initPulse();
}

VolumeControlPrivate::~VolumeControlPrivate()
{
    //m_instance = NULL;
}

void VolumeControlPrivate::initPulse()
{
        m_mainloop = pa_glib_mainloop_new(NULL);

        m_context = pa_context_new(
                        pa_glib_mainloop_get_api(m_mainloop),
                        "com.meego.settings.volume");

        if (pa_context_connect(m_context,
                                   NULL,
                                   PA_CONTEXT_NOFAIL, NULL) < 0)
        {
                qCritical("PulseAudioService: pa_context_connect() failed");
                cleanup();
                return;
        }

        pa_context_set_state_callback(m_context, contextStateCallback, this);
}

void VolumeControlPrivate::setSubscribeCallback()
{
        pa_context_set_subscribe_callback(m_context, subscribedEventsCallback, this);
        pa_operation *o;
        if (!(o = pa_context_subscribe(m_context, (pa_subscription_mask_t)
                (PA_SUBSCRIPTION_MASK_SINK|
                 PA_SUBSCRIPTION_MASK_SOURCE|
                 PA_SUBSCRIPTION_MASK_SINK_INPUT|
                 PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT|
                 PA_SUBSCRIPTION_MASK_CLIENT|
                 PA_SUBSCRIPTION_MASK_SERVER|
                 PA_SUBSCRIPTION_MASK_CARD), NULL, NULL)))
        {
                qWarning("pa_context_subscribe() failed");
        }

        pa_operation_unref(o);
}

void VolumeControlPrivate::updateSink()
{

        pa_operation *o;
        if(!sink())
        {
                o = pa_context_get_sink_info_list(m_context, pulseSinkInfoCallback, this);
        }
        else
        {
                o = pa_context_get_sink_info_by_name(m_context,
                                                         m_instance->sink()->name().toAscii().constData(),
                                                         pulseSinkInfoCallback, this);
        }

        pa_operation_unref(o);
}

void pulseSinkInfoCallback(pa_context *c, const pa_sink_info *info, int eol, void *)
{
        Q_UNUSED(c);
        Q_UNUSED(eol);

        if(info && !m_instance->sink())
        {
                m_instance->setSink(info);
                m_instance->update();
        }

        else if(info && QString(info->name) == m_instance->sink()->name())
        {
                m_instance->sink()->update(info);
                m_instance->update();
        }

}

void contextStateCallback(pa_context* c, void* )
{
        pa_context_state_t state = pa_context_get_state(c);
        if( state == PA_CONTEXT_READY)
        {
                m_instance->updateSink();
                m_instance->setSubscribeCallback();
        }
}

void subscribedEventsCallback(pa_context *c, enum pa_subscription_event_type t, uint32_t index, void *)
{
        Q_UNUSED(c);
        Q_UNUSED(index);
        switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK)
        {
                case PA_SUBSCRIPTION_EVENT_SINK:
                {
                        if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE)
                        {
                                //TODO handle the remove event
                        }
                        else if (((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW))
                        {
                        }
                        else
                        {
                                m_instance->updateSink();
                        }

                        break;
                }
        }
}

void VolumeControlPrivate::update()
{
        if(sink() == NULL)
            return;

        pa_cvolume tempvol = sink()->volume();
        pa_volume_t vol = pa_cvolume_avg(&tempvol);
        double volval = ((double) vol * 100) / PA_VOLUME_NORM;

        muteChanged(sink()->mute());
        m_volume = volval;
        volumeChanged(volval);
}

void VolumeControlPrivate::cleanup()
{
        pa_context_unref(m_context);
        pa_glib_mainloop_free(m_mainloop);
}

void VolumeControlPrivate::setVolume(int percent)
{
        if(sink() == NULL)
            return;

        pa_volume_t new_volume = (pa_volume_t) ((percent * PA_VOLUME_NORM) / 100);
        pa_cvolume dev_vol;
        pa_cvolume_set(&dev_vol, sink()->volume().channels, new_volume);
        pa_operation_unref(pa_context_set_sink_volume_by_name(m_context,
                                                                  sink()->name().toAscii().constData(),
                                                                  &dev_vol, NULL, NULL));
}

void VolumeControlPrivate::mute(bool muted)
{
        if (sink() != NULL) {
                pa_operation_unref(pa_context_set_sink_mute_by_name(m_context, sink()->name().toAscii().data(),
                                                                                                                        muted, NULL, NULL));
        }
}

